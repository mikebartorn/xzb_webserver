cmake_minimum_required(VERSION 2.8) #设定最小版本号
project(web)                        #设定工程文件名称
include_directories(./include)      #设定头文件包含目录
include_directories(/usr/include/mysql)

aux_source_directory(./src SRC_LIST)#源文件
SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-pthread") #包含pthread静态库
add_executable(main ${SRC_LIST})
target_link_libraries(main mysqlclient)
