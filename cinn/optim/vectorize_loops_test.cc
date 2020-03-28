#include "cinn/optim/vectorize_loops.h"

#include <gtest/gtest.h>

#include "cinn/cinn.h"
#include "cinn/common/ir.h"
#include "cinn/ir/ir_operators.h"
#include "cinn/lang/placeholder.h"
#include "cinn/optim/ir_simplify.h"
#include "cinn/optim/transform_polyfor_to_for.h"
#include "cinn/utils/string.h"

namespace cinn {
namespace optim {
using namespace ir;  // NOLINT
using utils::GetStreamCnt;
using utils::Trim;

TEST(VectorizeLoops, Split_sperate) {
  const int M  = 100;
  const int K  = 200;
  const int N  = 500;
  const int bn = 32;
  Placeholder<float> A("A", {M, K});
  Placeholder<float> B("B", {K, N});

  // C = A * B
  lang::Buffer C_buf(Float(32));
  Var k(K, "k");

  Tensor C = Compute(
      {M, N}, [&](Var i, Var j) { return lang::Sum(A(i, k) * B(k, j), k); }, "C", k);
  C->Bind(C_buf);

  {
    poly::Iterator i_outer, i_inner, j_outer, j_inner, k_outer, k_inner;
    std::tie(i_outer, i_inner, j_outer, j_inner) = C->stage()->Tile(0, 1, bn, bn);
    std::tie(k_outer, k_inner)                   = C->stage()->Split(poly::Iterator("k"), 8);
    C->stage()->Reorder({i_outer, j_outer, k_outer, k_inner, i_inner, j_inner});
    C->stage()->Split(j_inner, 8, poly::SplitRestStrategy::kAuto);
  }

  // Code gen
  auto funcs = Lower("matmul", {A, B, C});
  ASSERT_EQ(funcs.size(), 1UL);

  Target target;
  target.arch = Target::Arch ::X86;
  target.bits = Target::Bit ::k32;
  target.os   = Target::OS ::Linux;

  optim::VectorizeLoops(&funcs[0]->body, target);

  lang::Module module("module1", target);
  module.Append(funcs.front());
  module.Append(C_buf);

  CodeGenC codegen(target);
  codegen.SetInlineBuiltinCodes(false);
  auto out = codegen.Compile(module, CodeGenC::OutputKind::CImpl);

  auto target_out = R"ROC(
#include <cinn_runtime.h>
#include <stdio.h>

cinn_buffer_t* _C = cinn_buffer_t::new_((cinn_device_kind_t)(0)/*target*/, cinn_float32_t(), { 100, 500 });
void matmul(const struct cinn_buffer_t *_A, const struct cinn_buffer_t *_B, struct cinn_buffer_t *_C)
{
  cinn_buffer_malloc((void*)(0), _C);
  const float* A = (const float*)(cinn_buffer_get_data_const_handle(_A));
  const float* B = (const float*)(cinn_buffer_get_data_const_handle(_B));
  float* C = (float*)(cinn_buffer_get_data_handle(_C));
  {
    for (int32_t i_outer = 0; i_outer < 3; i_outer += 1) {
      for (int32_t j_outer = 0; j_outer < 15; j_outer += 1) {
        for (int32_t k_outer = 0; k_outer < 25; k_outer += 1) {
          for (int32_t k_inner = 0; k_inner < 8; k_inner += 1) {
            for (int32_t i_inner = 0; i_inner < 32; i_inner += 1) {
              for (int32_t j_inner_outer = 0; j_inner_outer < 4; j_inner_outer += 1) {
                for (int32_t j_inner_inner = 0; j_inner_inner < min(8, (500 + ((-8 * j_inner_outer) + (-32 * j_outer)))); j_inner_inner += 1) {
                  C[((((32 * i_outer) + i_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))] = (C[((((32 * i_outer) + i_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))] + (A[((((32 * i_outer) + i_inner) * 200) + ((8 * k_outer) + k_inner))] * B[((((8 * k_outer) + k_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))]));
                };
              };
            };
          };
        };
      };
      for (int32_t j_outer = 15; j_outer < 16; j_outer += 1) {
        for (int32_t k_outer = 0; k_outer < 25; k_outer += 1) {
          for (int32_t k_inner = 0; k_inner < 8; k_inner += 1) {
            for (int32_t i_inner = 0; i_inner < 32; i_inner += 1) {
              for (int32_t j_inner_outer = 0; j_inner_outer < (63 + (-4 * j_outer)); j_inner_outer += 1) {
                for (int32_t j_inner_inner = 0; j_inner_inner < min(8, (500 + ((-8 * j_inner_outer) + (-32 * j_outer)))); j_inner_inner += 1) {
                  C[((((32 * i_outer) + i_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))] = (C[((((32 * i_outer) + i_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))] + (A[((((32 * i_outer) + i_inner) * 200) + ((8 * k_outer) + k_inner))] * B[((((8 * k_outer) + k_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))]));
                };
              };
            };
          };
        };
      };
    };
    for (int32_t i_outer = 3; i_outer < 4; i_outer += 1) {
      for (int32_t j_outer = 0; j_outer < 15; j_outer += 1) {
        for (int32_t k_outer = 0; k_outer < 25; k_outer += 1) {
          for (int32_t k_inner = 0; k_inner < 8; k_inner += 1) {
            for (int32_t i_inner = 0; i_inner < (100 + (-32 * i_outer)); i_inner += 1) {
              for (int32_t j_inner_outer = 0; j_inner_outer < 4; j_inner_outer += 1) {
                for (int32_t j_inner_inner = 0; j_inner_inner < min(8, (500 + ((-8 * j_inner_outer) + (-32 * j_outer)))); j_inner_inner += 1) {
                  C[((((32 * i_outer) + i_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))] = (C[((((32 * i_outer) + i_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))] + (A[((((32 * i_outer) + i_inner) * 200) + ((8 * k_outer) + k_inner))] * B[((((8 * k_outer) + k_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))]));
                };
              };
            };
          };
        };
      };
      for (int32_t j_outer = 15; j_outer < 16; j_outer += 1) {
        for (int32_t k_outer = 0; k_outer < 25; k_outer += 1) {
          for (int32_t k_inner = 0; k_inner < 8; k_inner += 1) {
            for (int32_t i_inner = 0; i_inner < (100 + (-32 * i_outer)); i_inner += 1) {
              for (int32_t j_inner_outer = 0; j_inner_outer < (63 + (-4 * j_outer)); j_inner_outer += 1) {
                for (int32_t j_inner_inner = 0; j_inner_inner < min(8, (500 + ((-8 * j_inner_outer) + (-32 * j_outer)))); j_inner_inner += 1) {
                  C[((((32 * i_outer) + i_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))] = (C[((((32 * i_outer) + i_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))] + (A[((((32 * i_outer) + i_inner) * 200) + ((8 * k_outer) + k_inner))] * B[((((8 * k_outer) + k_inner) * 500) + (((32 * j_outer) + (8 * j_inner_outer)) + j_inner_inner))]));
                };
              };
            };
          };
        };
      };
    };
  };
}
)ROC";

  EXPECT_EQ(utils::Trim(target_out), utils::Trim(out));
}

TEST(Vectorize, replace_var) {
  using namespace ir;  // NOLINT

  const int M  = 100;
  const int K  = 200;
  const int N  = 500;
  const int bn = 32;
  Placeholder<float> A("A", {M, N});
  Placeholder<float> B("B", {M, N});

  // C = A * B
  lang::Buffer C_buf(Float(32));

  Tensor C = Compute(
      {M, N}, [&](Var i, Var j) { return A(i, j) * B(i, j); }, "C");
  C->Bind(C_buf);

  C->stage()->Vectorize(1, 16);

  auto funcs = Lower("matmul", {A, B, C});
  CHECK_EQ(funcs.size(), 1UL);

  optim::TransformPolyForToFor(&funcs[0]->body);

  detail::Vectorize(ir::_Var_::Make("j_inner", Int(32)), 16, &funcs.front()->body);

  Target target;
  target.arch = Target::Arch ::X86;
  target.bits = Target::Bit ::k32;
  target.os   = Target::OS ::Linux;

  lang::Module module("module1", target);
  module.Append(funcs[0]);

  CodeGenC codegen(target);
  codegen.SetInlineBuiltinCodes(false);
  auto out        = codegen.Compile(module, CodeGenC::OutputKind::CImpl);
  auto target_out = R"ROC(
#include <cinn_runtime.h>
#include <stdio.h>

void matmul(const struct cinn_buffer_t *_A, const struct cinn_buffer_t *_B, struct cinn_buffer_t *_C)
{
  cinn_buffer_malloc((void*)(0), _C);
  const float* A = (const float*)(cinn_buffer_get_data_const_handle(_A));
  const float* B = (const float*)(cinn_buffer_get_data_const_handle(_B));
  float* C = (float*)(cinn_buffer_get_data_handle(_C));
  for (int32_t i = 0; i < 100; i += 1) {
    for (int32_t j_outer = 0; j_outer < 31; j_outer += 1) {
      for (int32_t j_inner = 0; j_inner < 16; j_inner += 1) {
        C[StackVec<16,int32_t>::Ramp(((i * 500) + ((16 * j_outer) + 0)), 1, 16)] = (StackedVec<float,16>::Load(A,((i * 500) + ((16 * j_outer) + 0))) * StackedVec<float,16>::Load(B,((i * 500) + ((16 * j_outer) + 0))));
      };
    };
    for (int32_t j_outer = 31; j_outer < 32; j_outer += 1) {
      for (int32_t j_inner = 0; j_inner < (500 + (-16 * j_outer)); j_inner += 1) {
        C[StackVec<16,int32_t>::Ramp(((i * 500) + ((16 * j_outer) + 0)), 1, 16)] = (StackedVec<float,16>::Load(A,((i * 500) + ((16 * j_outer) + 0))) * StackedVec<float,16>::Load(B,((i * 500) + ((16 * j_outer) + 0))));
      };
    };
  };
}
)ROC";
  EXPECT_EQ(Trim(out), Trim(target_out));
}

TEST(Vectorize, TestMarkVectorize) {
  // create two forloops, check only one forloop is marked Vectorize.
  Context::Global().info_rgt().Clear();

  using namespace ir;  // NOLINT

  const int M  = 100;
  const int K  = 200;
  const int N  = 500;
  const int bn = 32;

  Target target;
  target.arch = Target::Arch ::X86;
  target.bits = Target::Bit ::k32;
  target.os   = Target::OS ::Linux;

  Placeholder<float> A("A", {M, N});
  Placeholder<float> B("B", {M, N});

  // C = A * B
  lang::Buffer C_buf(Float(32));

  Tensor C = Compute(
      {M, N}, [&](Var i, Var j) { return A(i, j) * B(i, j); }, "C");
  C->Bind(C_buf);

  Tensor D = Compute(
      {M, N}, [&](Var i, Var j) { return A(i, j) * B(i, j); }, "D");
  D->Bind(C_buf);

  // vectorize C, not D
  C->stage()->Vectorize(1, 16);

  auto funcs = Lower("matmul", {A, B, C, D});
  CHECK_EQ(funcs.size(), 1UL);

  std::cout << "before optim\n" << funcs.front()->body << std::endl;

  optim::TransformPolyForToFor(&funcs[0]->body);
  optim::VectorizeLoops(&funcs[0]->body, target);
  optim::Simplify(&funcs[0]->body);

  lang::Module module("module1", target);
  module.Append(funcs[0]);

  CodeGenC codegen(target);
  codegen.SetInlineBuiltinCodes(false);
  auto out = codegen.Compile(module, CodeGenC::OutputKind::CImpl);
  std::cout << "out:\n" << out;

  auto target_out = R"ROC(
#include <cinn_runtime.h>
#include <stdio.h>

void matmul(const struct cinn_buffer_t *_A, const struct cinn_buffer_t *_B, struct cinn_buffer_t *_C)
{
  cinn_buffer_malloc((void*)(0), _C);
  const float* A = (const float*)(cinn_buffer_get_data_const_handle(_A));
  const float* B = (const float*)(cinn_buffer_get_data_const_handle(_B));
  float* C = (float*)(cinn_buffer_get_data_handle(_C));
  float* D = (float*)(cinn_buffer_get_data_handle(_C));
  for (int32_t i = 0; i < 100; i += 1) {
    for (int32_t j_outer = 0; j_outer < 31; j_outer += 1) {
      C[StackVec<16,int32_t>::Ramp(((500 * i) + (16 * j_outer)), 1, 16)] = (StackedVec<float,16>::Load(A,((500 * i) + (16 * j_outer))) * StackedVec<float,16>::Load(B,((500 * i) + (16 * j_outer))));
    };
    for (int32_t j_outer = 31; j_outer < 32; j_outer += 1) {
      for (int32_t j_inner = 0; j_inner < (500 + (-16 * j_outer)); j_inner += 1) {
        C[((500 * i) + ((16 * j_outer) + j_inner))] = (A[((500 * i) + ((16 * j_outer) + j_inner))] * B[((500 * i) + ((16 * j_outer) + j_inner))]);
      };
    };
  };
  for (int32_t i = 0; i < 100; i += 1) {
    for (int32_t j = 0; j < 500; j += 1) {
      D[((500 * i) + j)] = (A[((500 * i) + j)] * B[((500 * i) + j)]);
    };
  };
}
)ROC";

  EXPECT_EQ(Trim(out), Trim(target_out));
  EXPECT_EQ(Context::Global().info_rgt().Get<int>("vectorized_forloop_count"), 1);
}

TEST(Vectorize, basic) {
  Var x("x");
  Var y("y");
  Var z("z");
  Var k("k");

  {
    Expr expr = x;
    detail::Vectorize(z, 8, &expr);
    EXPECT_EQ(GetStreamCnt(expr), "x");
  }

  {
    Expr expr = z;
    detail::Vectorize(z, 8, &expr);
    EXPECT_EQ(GetStreamCnt(expr), "Ramp(0,1,8)");
  }

  {
    Expr expr = x + z + 1;
    detail::Vectorize(z, 8, &expr);
    EXPECT_EQ(GetStreamCnt(expr), "Ramp((1 + (x + 0)),1,8)");
    LOG(INFO) << "expr " << expr;
  }

  {
    Expr expr = x * 2 + z * 3 + 1;
    detail::Vectorize(z, 8, &expr);
    EXPECT_EQ(GetStreamCnt(expr), "Ramp((1 + ((x * 2) + (0 * 3))),(1 * 3),8)");
  }

  {
    Expr expr = x * 2 + z + 1;
    detail::Vectorize(z, 8, &expr);
    EXPECT_EQ(GetStreamCnt(expr), "Ramp((1 + ((x * 2) + 0)),1,8)");
  }
  {
    Expr expr = (k + ((32768 * x) + ((32 * y) + (128 * z))));
    detail::Vectorize(k, 8, &expr);
    EXPECT_EQ(GetStreamCnt(expr), "Ramp((((32768 * x) + ((32 * y) + (128 * z))) + 0),1,8)");
  }

  lang::Placeholder<float> A("A", {10, 10});
  {
    Expr expr = Load::Make(Expr(Tensor(A)), x + 10 * y + z) + 10.f;
    detail::Vectorize(x, 8, &expr);
    LOG(INFO) << "expr " << expr;
  }

  {
    Var v0("v", Float(32));
    Expr body = v0 + Load::Make(Expr(Tensor(A)), x + 10 * y + z) + 10.f;
    Expr expr =
        For::Make(z, common::make_const(0), common::make_const(8), ForType::Vectorized, DeviceAPI::UNK, body);
    Target target;
    VectorizeLoops(&expr, target);
    LOG(INFO) << "expr " << expr;
  }
}

}  // namespace optim
}  // namespace cinn