cd ./build
make -j
cd ..

setarch x86_64 -R ./build/test/mcsim-unittest -mdfile ../Apps/md/test-md.toml -runfile ../Apps/list/test-applist.toml
