# 简易c/s通信（10/14）  
## 使用说明：  
1.本地测试：（client.cpp文件中line28，server端地址为本机地址127.0.0.1）  
  ——linux下将两个c++文件用g++编译，命令：```g++ server.cpp -o server  g++ client.cpp -o client```；  
  ——在对应目录里找到两个生成的程序server和client并运行，命令：```(sudo) ./server  ./client```；  
  ——在client端输入相应子串server端即可得到响应；  
2.局域网下不同主机测试：（client.cpp文件中line28，server端地址为对应server端主机ip地址）  
  ——确保两台主机在同一局域网中，作server端主机查本机ip地址，命令：```ifconfig```（linux）/```ipconfig```（Windows），client端修改文件对应地址并ping该ip地址保证连通；  
  ——注意防火墙策略，如果不通可能是被防火墙过滤，关闭防火墙命令：```service iptables stop```；  
  ——其他步骤参考本地测试时步骤；    
  
# 基于c/s的广播通信（10/19）
## 使用说明：（利用mininet进行）
1.linux下将两个c++文件用g++编译，命令：```g++ broadcast_client.cpp -o b_client  g++ broadcast_server.cpp -o b_server```；  
2.启动mininet并创建1交换机3主机的网络拓扑（s1-h1,s1-h2,s1-h3），命令：```sudo mn --topo single,3```；    
3.用命令：```xterm h1 h2 h3```打开3主机的终端，3主机地址分别为10.0.0.1/255.0.0.0，10.0.0.2/255.0.0.0，10.0.0.3/255.0.0.0， 可用route命令查询各自路由表；  
4.将h1作为server端，h2和h3作为client端，server端一定要有默认网关，否则会出现“network is unreachable”的问题，该实验中添加的默认网关应为10.255.255.254，命令：```route add -net default gw 10.255.255.254```；     
5.在xterm终端运行各自程序，h1运行b_server，h2和h3运行b_client，命令：```./b_server  ./b_client```；   

# 基于c/s的组播通信（10/27）    
## 使用说明：   
1.（同局域网下1switch3host实验）过程参照基于c/s的广播通信实验，注意server端和client端均要设置默认网关；  
2.同局域网下实验若加入广播实验中的client端（端口号相同），也可收到server端发送的组播消息；  

# 代理服务器（12/04）
## 使用说明：
1.两台主机在同一局域网下（ping命令互查），虚拟机网络适配器设置为桥接模式（NAT模式无法保证在一个子网）；  
2.linux下将文件用g++编译，命令：```g++ CDN_server.cpp -o CDN_server```；  
3.作为代理服务器的主机在程序存放的目录下运行程序，命令：```./CDN_server```，另一台主机启动浏览器勾选代理设置参数（端口号为11111，地址为代理服务器主机ip地址），服务器一端会生成proxy.log文件记录信息；  
4.测试了四个网址：[by.cuc.edu.cn](by.cuc.edu.cn) ; www.sina.com.cn ; www.qq.com ; www.baidu.com; 前三个均成功打开，只有尝试打开百度时报出了"The proxy server is refusing connections"的错误；
