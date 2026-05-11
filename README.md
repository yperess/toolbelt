# Getting started

## Modifying python dependencies
1. Update `env_setup/py/env_setup/virtualenv_setup/requirements.txt`
2. Run `bazelisk run //env_setup/py:update_requirements`

## Build the documentation
```
$ bazelisk build //docs
```

The output will be seen in `bazel-out/k8-fastbuild/bin/docs/docs/_build/html/index.html`

## Launch the documentation
```
$ bazelisk run //docs:open
```

## Run CMake tests

Activate the environment first (required once per shell session):
```
$ source ./activate.sh
```

Configure the build (required once, or after CMakeLists changes):
```
$ cmake -B out -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

Run all tests in a module group:
```
$ ninja -C out pw_run_tests.toolbelt
```

Run a single test:
```
$ ninja -C out toolbelt.polling_debounce_test.run
```

Build tests without running:
```
$ ninja -C out pw_tests.toolbelt
```
