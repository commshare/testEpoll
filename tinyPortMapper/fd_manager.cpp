/*
 * fd_manager.cpp
 *
 *  Created on: Sep 25, 2017
 *      Author: root
 */


#include "fd_manager.h"
fd_manager_t fd_manager;

int fd_manager_t::fd_exist(int fd)
{
	/*map的查找*/
	return fd_to_fd64_mp.find(fd)!=fd_to_fd64_mp.end();
}
int fd_manager_t::exist(fd64_t fd64)
{
	return fd64_to_fd_mp.find(fd64)!=fd64_to_fd_mp.end();
}
int fd_manager_t::to_fd(fd64_t fd64)
{
	assert(exist(fd64));
	return fd64_to_fd_mp[fd64];
}

void fd_manager_t::fd64_close(fd64_t fd64)
{
	assert(exist(fd64));
	int fd=fd64_to_fd_mp[fd64];
	fd64_to_fd_mp.erase(fd64);
	fd_to_fd64_mp.erase(fd);
	if(exist_info(fd64))
	{
		fd_info_mp.erase(fd64);
	}
	assert(close(fd)==0);
}
void fd_manager_t::reserve(int n)
{
	/*
	 * void reserve ( size_type n );
Request a capacity change
Sets the number of buckets in the container (bucket_count) to the most appropriate to contain at least n elements.

If n is greater than the current bucket_count multiplied by the max_load_factor, the container's bucket_count is increased and a rehash is forced.

If n is lower than that, the function may have no effect.
	 * */
	fd_to_fd64_mp.reserve(n); /*改变容量*/
	fd64_to_fd_mp.reserve(n);
	fd_info_mp.reserve(n);
}
u64_t fd_manager_t::create(int fd)
{
	assert(!fd_exist(fd));
	fd64_t fd64=counter++; /*原来是内部计数器一直加1啊*/
	fd_to_fd64_mp[fd]=fd64;
	fd64_to_fd_mp[fd64]=fd;
	return fd64;
}
fd_manager_t::fd_manager_t()
{
	/*初始值竟然是这样的TODO */
	counter=u32_t(-1);
	counter+=100;
	reserve(10007);
}
fd_info_t & fd_manager_t::get_info(fd64_t fd64)
{
	assert(exist(fd64));
	return fd_info_mp[fd64];
}
int fd_manager_t::exist_info(fd64_t fd64)
{
	return fd_info_mp.find(fd64)!=fd_info_mp.end();
}
