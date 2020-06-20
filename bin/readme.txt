marc_client:
	marc：框架运行程序
	client.ini: client节点运行配置文件
	myzip：压缩程序（被marc调用）
	marc_cdmn.sh：检测CPU/磁盘/内存/网卡状态的脚本（被marc调用）
	killpro.bat/sh：用于marc框架在探测到应用程序（如采集器）运行超时后杀死应用程序的脚本（被marc调用）
	killprolist.ini：killpro.bat/sh的配置文件
	Test.bat/sh：测试用的client节点应用程序，实际部署环境下可删掉
	zlib1.dll：MARC部署在windows下运行时依赖的zlib库
marc_master:
	marc：框架运行程序
	master.ini: master节点运行配置文件
	http文件夹：master节点启动http server时所需要的文件夹
	marc_cdmn.sh：检测CPU/磁盘/内存/网卡状态的脚本（被marc调用）
	myzip：压缩程序（被marc调用）
	TaskCreateTest.bat/sh：测试用的master节点应用程序（被marc调用），实际部署环境下可删掉
	zlib1.dll：MARC部署在windows下运行时依赖的zlib库
marc_result：
	marc：框架运行程序
	result.ini: master节点运行配置文件
	myzip：压缩程序（被marc调用）
	marc_cdmn.sh：检测CPU/磁盘/内存/网卡状态的脚本（被marc调用）
	ResultStoreTest.bat/sh：测试用的result节点应用程序（被marc调用），实际部署环境下可删掉
	zlib1.dll：MARC部署在windows下运行时依赖的zlib库

备注：
	上述各个节点下的marc,myzip,marc_cdmn.sh其实都是相同的。