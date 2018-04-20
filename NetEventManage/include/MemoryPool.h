#ifndef __CMEMORYPOOL__H
#define __CMEMORYPOOL__H

// �ڴ��ģ��
#include <list>
#include <vector>
#include <iostream>
#include <mutex>
#include <algorithm>
#include <thread>
template<class T>
struct TBuff
{
	T* pBuff; // ָ��
	int nCount;
	TBuff()
	{
		pBuff = nullptr;
		nCount = 0;
	}
};

template<class T>
class CMemoryPool
{
public:
	CMemoryPool();
	~CMemoryPool();

public:
	T* GetElement(); // ��ȡһ��ָ�� ʧ�ܷ���nullptr
	void ReleaseElement(T*); // �Ż�һ��ָ��

private:
	std::list<T*> m_lstFree; // ���е��ڴ������
	std::list<T*> m_lstUse; // ����ʹ�õ��ڴ������
	unsigned int  m_nElementCount;
	std::vector<TBuff<T>> m_vectBuff; // ���з�����ڴ� ��������ʱ�ͷ�

	std::mutex m_mutexFree; // ������ ��֤�̰߳�ȫ
	std::mutex m_mutexUse; // ʹ���� ��֤�̰߳�ȫ

};

template<class T>
CMemoryPool<T>::CMemoryPool()
{
	m_nElementCount = 1; // ��ʼ��Ϊ1 
	TBuff<T> obj;
	obj.nCount = 1;
	obj.pBuff = new (std::nothrow) T[obj.nCount]; // ����ʧ�ܲ��׳��쳣���ؿ�ָ��
	m_vectBuff.push_back(obj);
	// ��������ڴ������ж���
	m_lstFree.push_back(obj.pBuff);
}

template<class T>
CMemoryPool<T>::~CMemoryPool()
{
	while (1) // �ȴ����е��ڴ�ȫ��ʹ����
	{
		m_mutexUse.lock();
		if (m_lstUse.empty())
		{
			for (auto & obj : m_vectBuff)
				delete[] obj.pBuff;
			m_mutexUse.unlock();
			break;
		}
		else
		{
			m_mutexUse.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(20)); // ˯��һ��ʱ���ټ��
		}
	}
}

template<class T>
T* CMemoryPool<T>::GetElement()
{
	m_mutexFree.lock();
	if (m_lstFree.empty()) // û�п��е��ڴ�
	{
		TBuff<T> obj;
		obj.nCount = 2 * m_nElementCount; // ��һ�η��������
		obj.pBuff = new (std::nothrow)T[obj.nCount];
		for (int i = 0; i < obj.nCount; ++i)
			m_lstFree.push_back(obj.pBuff + i);
		m_vectBuff.push_back(obj);
	}
	T* pHead = m_lstFree.front();
	m_lstFree.pop_front();
	m_mutexFree.unlock();
	m_mutexUse.lock();
	m_lstUse.push_back(pHead);
	m_mutexUse.unlock();
	return pHead;
}

template<class T>
void CMemoryPool<T>::ReleaseElement(T* pData)
{
	m_mutexUse.lock();
	auto iter = std::find_if(m_lstUse.begin(), m_lstUse.end(), [pData](T* pFind) { return pData == pFind; }); // �ҵ���ָ��
	if (iter != m_lstUse.end())
		m_lstUse.erase(iter);
	m_mutexUse.unlock();
	m_mutexFree.lock();
	m_lstFree.push_back(pData);
	m_mutexFree.unlock();
}

#endif
