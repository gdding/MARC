make clean
./configure CXXFLAGS="-O2" --prefix=$(pwd)
make
make install
cp -f $(pwd)/bin/myzip $(pwd)/bin/marc_master/
cp -f $(pwd)/bin/marc $(pwd)/bin/marc_master/
cp -f $(pwd)/bin/myzip $(pwd)/bin/marc_result/
cp -f $(pwd)/bin/marc $(pwd)/bin/marc_result/
cp -f $(pwd)/bin/myzip $(pwd)/bin/marc_client/
cp -f $(pwd)/bin/marc $(pwd)/bin/marc_client/
rm $(pwd)/bin/marc
rm $(pwd)/bin/myzip
