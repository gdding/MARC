echo TaskDirPath=%1 >%1\TaskDirPath.txt
echo ClientID=%2 >%1\ClientID.txt
copy d:\test.dat %1\
echo test >%1\.success