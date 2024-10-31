#include "MemoryDescriptor.h"
#include "Kernel.h"
#include "PageManager.h"
#include "Machine.h"
#include "PageDirectory.h"
#include "Video.h"

void MemoryDescriptor::Initialize()
{
	this->m_UserPageTableArray = NULL;
}

void MemoryDescriptor::Release()
{
	KernelPageManager& kernelPageManager = Kernel::Instance().GetKernelPageManager();
	if ( this->m_UserPageTableArray )
	{
		kernelPageManager.FreeMemory(sizeof(PageTable) * USER_SPACE_PAGE_TABLE_CNT, (unsigned long)this->m_UserPageTableArray - Machine::KERNEL_SPACE_START_ADDRESS);
		this->m_UserPageTableArray = NULL;
	}
}

unsigned int MemoryDescriptor::MapEntry(unsigned long virtualAddress, unsigned int size, unsigned long phyPageIdx, bool isReadWrite)
{	
	return 0;
}

void MemoryDescriptor::MapTextEntrys(unsigned long textStartAddress, unsigned long textSize, unsigned long textPageIdx)
{
	this->MapEntry(textStartAddress, textSize, textPageIdx, false);
}
void MemoryDescriptor::MapDataEntrys(unsigned long dataStartAddress, unsigned long dataSize, unsigned long dataPageIdx)
{
	this->MapEntry(dataStartAddress, dataSize, dataPageIdx, true);
}

void MemoryDescriptor::MapStackEntrys(unsigned long stackSize, unsigned long stackPageIdx)
{
	unsigned long stackStartAddress = (USER_SPACE_START_ADDRESS + USER_SPACE_SIZE - stackSize) & 0xFFFFF000;
	this->MapEntry(stackStartAddress, stackSize, stackPageIdx, true);
}

PageTable* MemoryDescriptor::GetUserPageTableArray()
{
	return this->m_UserPageTableArray;
}
unsigned long MemoryDescriptor::GetTextStartAddress()
{
	return this->m_TextStartAddress;
}
unsigned long MemoryDescriptor::GetTextSize()
{
	return this->m_TextSize;
}
unsigned long MemoryDescriptor::GetDataStartAddress()
{
	return this->m_DataStartAddress;
}
unsigned long MemoryDescriptor::GetDataSize()
{
	return this->m_DataSize;
}
unsigned long MemoryDescriptor::GetStackSize()
{
	return this->m_StackSize;
}

bool MemoryDescriptor::EstablishUserPageTable( unsigned long textVirtualAddress, unsigned long textSize, unsigned long dataVirtualAddress, unsigned long dataSize, unsigned long stackSize )
{
	User& u = Kernel::Instance().GetUser();

	/* 如果超出允许的用户程序最大8M的地址空间限制 */
	if ( textSize + dataSize + stackSize  + PageManager::PAGE_SIZE > USER_SPACE_SIZE - textVirtualAddress)
	{
		u.u_error = User::ENOMEM;
		Diagnose::Write("u.u_error = %d\n",u.u_error);
		return false;
	}

	this->MapToPageTable();
	return true;
}

void MemoryDescriptor::ClearUserPageTable()
{

}

#define GETUSERPAGEINDEX(x) ((x) >> 22)
#define GETPAGENUM(x) ( ((x) + PageManager::PAGE_SIZE -1) / PageManager::PAGE_SIZE)
#define GETSTARTENTRY(x) (((x) & 0x003ff000) >> 12)

void MapUserTable(PageTable* pUserPageTable , bool is_readwriter , unsigned long start_address , unsigned long size , unsigned long pstart_address){
	unsigned long user_pageidx = GETUSERPAGEINDEX(start_address);
	unsigned long pagenum = GETPAGENUM(size); 
	unsigned long phystart = pstart_address >> 12;
	unsigned long start_entry = GETSTARTENTRY(start_address); 
	for ( unsigned long i = start_entry ; i < start_entry + pagenum ;i++){
		pUserPageTable[user_pageidx].m_Entrys[i].m_Present = 1;
		pUserPageTable[user_pageidx].m_Entrys[i].m_ReadWriter = is_readwriter; 
		pUserPageTable[user_pageidx].m_Entrys[i].m_PageBaseAddress = phystart + i - start_entry;	
	} 
}

void MemoryDescriptor::MapToPageTable()
{
	User& u = Kernel::Instance().GetUser();

	PageTable* pUserPageTable = Machine::Instance().GetUserPageTableArray();
	unsigned int textAddress = 0;
	if ( u.u_procp->p_textp != NULL )
	{
		textAddress = u.u_procp->p_textp->x_caddr;
	}
	unsigned int data_address = u.u_procp->p_addr;
	for (unsigned int i = 0; i < Machine::USER_PAGE_TABLE_CNT; i++)
	{
		for ( unsigned int j = 0; j < PageTable::ENTRY_CNT_PER_PAGETABLE; j++ )
		{
			pUserPageTable[i].m_Entrys[j].m_Present = 0;   //先清0
		}
	}
	// 首先映射正文段
	MapUserTable(pUserPageTable , 0 , m_TextStartAddress , m_TextSize , textAddress);
	// 和正文段一样,映射数据段
	MapUserTable(pUserPageTable , 1 , m_DataStartAddress , m_DataSize , data_address+ PageManager::PAGE_SIZE);
	// 最后映射栈
	unsigned long stackStartAddress = (USER_SPACE_START_ADDRESS + USER_SPACE_SIZE - m_StackSize) & 0xFFFFF000;
	MapUserTable(pUserPageTable , 1 , stackStartAddress , m_StackSize , data_address+(m_DataSize + 2*PageManager::PAGE_SIZE -1));


	pUserPageTable[0].m_Entrys[0].m_Present = 1;
	pUserPageTable[0].m_Entrys[0].m_ReadWriter = 1;
	pUserPageTable[0].m_Entrys[0].m_PageBaseAddress = 0;

	FlushPageDirectory();
}
