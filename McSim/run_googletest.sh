cd ./build
make -j 4
cd ..

setarch x86_64 -R ./build/test/mcsim-unittest -mdfile ../Apps/md/test-md.toml
