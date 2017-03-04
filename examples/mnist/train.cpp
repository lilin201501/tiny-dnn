/*
    Copyright (c) 2013, Taiga Nomi
    All rights reserved.

    Use of this source code is governed by a BSD-style license that can be found
    in the LICENSE file.
*/
#include <iostream>
#include "tiny_dnn/tiny_dnn.h"

using namespace tiny_dnn;
using namespace tiny_dnn::activation;

static void construct_net(network<sequential>& nn) {
// connection table [Y.Lecun, 1998 Table.1]
#define O true
#define X false
  // clang-format off
    static const bool tbl[] = {
        O, X, X, X, O, O, O, X, X, O, O, O, O, X, O, O,
        O, O, X, X, X, O, O, O, X, X, O, O, O, O, X, O,
        O, O, O, X, X, X, O, O, O, X, X, O, X, O, O, O,
        X, O, O, O, X, X, O, O, O, O, X, X, O, X, O, O,
        X, X, O, O, O, X, X, O, O, O, O, X, O, O, X, O,
        X, X, X, O, O, O, X, X, O, O, O, O, X, O, O, O
    };
// clang-format on
#undef O
#undef X

  // by default will use backend_t::tiny_dnn unless you compiled
  // with -DUSE_AVX=ON and your device supports AVX intrinsics
  core::backend_t backend_type = core::default_engine();

  // construct nets
  //
  // C : convolution
  // S : sub-sampling
  // F : fully connected
  nn << convolutional_layer(32, 32, 5, 1,
                            6,  // C1, 1@32x32-in, 6@28x28-out
                            padding::valid, true, 1, 1, backend_type)
     << tanh_layer(28, 28, 6)
     << average_pooling_layer(28, 28, 6,
                              2)  // S2, 6@28x28-in, 6@14x14-out
     << tanh_layer(14, 14, 6)
     << convolutional_layer(14, 14, 5, 6,
                            16,  // C3, 6@14x14-in, 16@10x10-out
                            connection_table(tbl, 6, 16), padding::valid, true,
                            1, 1, backend_type)
     << tanh_layer(10, 10, 16)
     << average_pooling_layer(10, 10, 16,
                              2)  // S4, 16@10x10-in, 16@5x5-out
     << tanh_layer(5, 5, 16)
     << convolutional_layer(5, 5, 5, 16,
                            120,  // C5, 16@5x5-in, 120@1x1-out
                            padding::valid, true, 1, 1, backend_type)
     << tanh_layer(1, 1, 120)
     << fully_connected_layer(120, 10, true,  // F6, 120-in, 10-out
                              backend_type)
     << tanh_layer(10);
}

static void train_lenet(const std::string& data_dir_path) {
  // specify loss-function and learning strategy
  network<sequential> nn;
  adagrad optimizer;

  construct_net(nn);

  std::cout << "load models..." << std::endl;

  // load MNIST dataset
  std::vector<label_t> train_labels, test_labels;
  std::vector<vec_t> train_images, test_images;

  parse_mnist_labels(data_dir_path + "/train-labels.idx1-ubyte", &train_labels);
  parse_mnist_images(data_dir_path + "/train-images.idx3-ubyte", &train_images,
                     -1.0, 1.0, 2, 2);
  parse_mnist_labels(data_dir_path + "/t10k-labels.idx1-ubyte", &test_labels);
  parse_mnist_images(data_dir_path + "/t10k-images.idx3-ubyte", &test_images,
                     -1.0, 1.0, 2, 2);

  std::cout << "start training" << std::endl;

  progress_display disp(static_cast<unsigned long>(train_images.size()));
  timer t;
  int minibatch_size = 16;
  int num_epochs     = 30;

  optimizer.alpha *=
    std::min(tiny_dnn::float_t(4),
             static_cast<tiny_dnn::float_t>(std::sqrt(minibatch_size)));

  // create callback
  auto on_enumerate_epoch = [&]() {
    std::cout << t.elapsed() << "s elapsed." << std::endl;
    tiny_dnn::result res = nn.test(test_images, test_labels);
    std::cout << res.num_success << "/" << res.num_total << std::endl;

    disp.restart(static_cast<unsigned long>(train_images.size()));
    t.restart();
  };

  auto on_enumerate_minibatch = [&]() { disp += minibatch_size; };

  // training
  nn.train<mse>(optimizer, train_images, train_labels, minibatch_size,
                num_epochs, on_enumerate_minibatch, on_enumerate_epoch);

  std::cout << "end training." << std::endl;

  // test and show results
  nn.test(test_images, test_labels).print_detail(std::cout);

  // save network model & trained weights
  nn.save("LeNet-model");
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage : " << argv[0] << " path_to_data (example:../data)"
              << std::endl;
    return -1;
  }
  train_lenet(argv[1]);
  return 0;
}
