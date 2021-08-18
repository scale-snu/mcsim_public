cd ./build
make -j
cd ..

setarch x86_64 -R ./build/test/mcsim-unittest -mdfile ./test/test-md.toml -runfile ./test/test-applist.toml
