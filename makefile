CXX = g++
target = webserver
$(target):main.cpp ./config/config.cpp ./http_con/http_con.cpp ./lock/locker.cpp ./timer/lst_timer.cpp ./utils/utils.cpp webserver.cpp
	$(CXX) -o $@ $^ -pthread

