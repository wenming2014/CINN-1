#!/usr/bin/env python3
import paddle as paddle
import paddle.fluid as fluid
from cinn.frontend import *
from cinn import Target
from cinn.framework import *
import unittest
import cinn
from cinn import runtime
from cinn import ir
from cinn import lang
from cinn.common import *
import numpy as np
import paddle.fluid as fluid
import sys
import time

enable_gpu = sys.argv.pop()
model_dir = sys.argv.pop()

# enable_gpu = "OFF"
# model_dir = "/home/wangyue50/CINN-my4/CINN4/build/thirds/ResNet18"
print("enable_gpu is : ", enable_gpu)
print("model_dir is : ", model_dir)


class TestLoadResnetModel(unittest.TestCase):
    def setUp(self):
        if enable_gpu == "ON":
            self.target = DefaultNVGPUTarget()
        else:
            self.target = DefaultHostTarget()
        self.model_dir = model_dir
        self.x_shape = [1, 3, 224, 224]
        self.target_tensor = 'save_infer_model/scale_0'
        self.input_tensor = 'image'

    def get_paddle_inference_result(self, model_dir, data):
        config = fluid.core.AnalysisConfig(model_dir + '/__model__',
                                           model_dir + '/params')
        config.disable_gpu()
        config.switch_ir_optim(False)
        self.paddle_predictor = fluid.core.create_paddle_predictor(config)
        data = fluid.core.PaddleTensor(data)
        results = self.paddle_predictor.run([data])
        get_tensor = self.paddle_predictor.get_output_tensor(
            self.target_tensor).copy_to_cpu()
        return get_tensor

    def apply_test(self):
        start = time.time()
        x_data = np.random.random(self.x_shape).astype("float32")
        self.executor = Interpreter([self.input_tensor], [self.x_shape])
        print("self.mode_dir is:", self.model_dir)
        # True means load combined model
        self.executor.load_paddle_model(self.model_dir, self.target, True)
        end1 = time.time()
        print("load_paddle_model time is: %.3f sec" % (end1 - start))
        a_t = self.executor.get_tensor(self.input_tensor)
        a_t.from_numpy(x_data, self.target)
        out = self.executor.get_tensor(self.target_tensor)
        out.from_numpy(np.zeros(out.shape(), dtype='float32'), self.target)
        end2 = time.time()
        self.executor.run()
        end3 = time.time()
        print("Preheat executor.run() time is: %.3f sec" % (end3 - end2))

        end4 = time.perf_counter()
        repeat = 1
        for i in range(repeat):
            self.executor.run()
        end5 = time.perf_counter()
        print("Repeat %d times, average Executor.run() time is: %.3f s" %
              (repeat, end5 - end4))

        # a_t.from_numpy(x_data, self.target)
        # out.from_numpy(np.zeros(out.shape(), dtype='float32'), self.target)
        # self.executor.run()

        out = out.numpy(self.target)
        target_result = self.get_paddle_inference_result(
            self.model_dir, x_data)

        print("result in test_model: \n")
        out = out.reshape(-1)
        target_result = target_result.reshape(-1)
        for i in range(0, out.shape[0]):
            if np.abs(out[i] - target_result[i]) > 1e-3:
                print("Error! ", i, "-th data has diff with target data:\n",
                      out[i], " vs: ", target_result[i], ". Diff is: ",
                      out[i] - target_result[i])
        self.assertTrue(np.allclose(out, target_result, atol=1e-3))

    def test_model(self):
        self.apply_test()


if __name__ == "__main__":
    unittest.main()
