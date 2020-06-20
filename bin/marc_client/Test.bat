echo AppName=%0 >%2\AppName.txt
echo ClientID=%3 >%2\ClientID.txt
tree c:\ >%2\tree_c.txt
copy %1\*.* %2\
echo test >.success