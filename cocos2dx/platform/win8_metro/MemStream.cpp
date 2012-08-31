/****************************************************************************
Copyright (c) 2012 Denis Mingulov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "MemStream.h"

#define BLOCK_SIZE 0x2000

MemStream::MemStream(int initialBufSize)
	: m_curPos(0), m_size(0), m_buf(0), m_bufSize(0)
{
	_refcount = 1;
	RequestResize(initialBufSize);
}

MemStream::~MemStream()
{
	delete[] m_buf;
}

HRESULT MemStream::Create(int initialBufSize, IStream ** ppStream)
{
	*ppStream = new MemStream(initialBufSize);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE MemStream::QueryInterface(REFIID iid, void ** ppvObject)
{ 
	if (iid == __uuidof(IUnknown)
		|| iid == __uuidof(IStream)
		|| iid == __uuidof(ISequentialStream))
	{
		*ppvObject = static_cast<IStream*>(this);
		AddRef();
		return S_OK;
	} 
	else
	{
		return E_NOINTERFACE; 
	}
}

ULONG STDMETHODCALLTYPE MemStream::AddRef(void) 
{ 
	return (ULONG)InterlockedIncrement(&_refcount); 
}

ULONG STDMETHODCALLTYPE MemStream::Release(void) 
{
	ULONG res = (ULONG) InterlockedDecrement(&_refcount);
	if (res == 0) 
		delete this;
	return res;
}

void MemStream::Resize(int requiredSize, bool allowLess)
{
	// minimal size = the current size
	if (requiredSize < m_size)
		requiredSize = m_size;

	// do nothing if the current size is enough
	if (!allowLess && requiredSize < m_bufSize)
		return;

	if (requiredSize == m_bufSize)
		return;

	BYTE *bufNext = new BYTE[requiredSize];
	if (m_size)
		memmove(bufNext, m_buf, m_size);

	delete[] m_buf;
	m_buf = bufNext;
	m_bufSize = requiredSize;
}

void MemStream::RequestResize(int requiredSize)
{
	// do resize if it is neccessary only
	if (requiredSize > m_bufSize)
		Resize(((requiredSize + BLOCK_SIZE) | (BLOCK_SIZE - 1)) + 1);
}

// ISequentialStream Interface
HRESULT STDMETHODCALLTYPE MemStream::Read(void* pv, ULONG cb, ULONG* pcbRead)
{
	int read = cb;
	if (m_curPos + cb > m_size)
		read = m_size - m_curPos;

	if (pv)
		memmove(pv, m_buf + m_curPos, read);

	m_curPos += read;

	if (pcbRead)
		*pcbRead = read;
	if (read < cb)
		return S_FALSE;
//        return (rc) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

HRESULT STDMETHODCALLTYPE MemStream::Write(void const* pv, ULONG cb, ULONG* pcbWritten)
{
	RequestResize(m_curPos + cb);
	memmove(m_buf + m_curPos, pv, cb);
	if (pcbWritten)
		*pcbWritten = cb;
	m_curPos += cb;
	if (m_curPos > m_size)
		m_size = m_curPos;
//        return rc ? S_OK : HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

// IStream Interface
HRESULT STDMETHODCALLTYPE MemStream::SetSize(ULARGE_INTEGER)
{ 
	return E_NOTIMPL;   
}
    
HRESULT STDMETHODCALLTYPE MemStream::CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*,
        ULARGE_INTEGER*) 
{ 
	return E_NOTIMPL;   
}
    
HRESULT STDMETHODCALLTYPE MemStream::Commit(DWORD)                                      
{ 
	return S_OK;
	//return E_NOTIMPL;   
}
    
HRESULT STDMETHODCALLTYPE MemStream::Revert(void)                                       
{ 
	return E_NOTIMPL;   
}
    
HRESULT STDMETHODCALLTYPE MemStream::LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)              
{ 
	return E_NOTIMPL;   
}
    
HRESULT STDMETHODCALLTYPE MemStream::UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)            
{ 
	return E_NOTIMPL;   
}
    
HRESULT STDMETHODCALLTYPE MemStream::Clone(IStream **)                                  
{ 
	return E_NOTIMPL;   
}

HRESULT STDMETHODCALLTYPE MemStream::Seek(LARGE_INTEGER liDistanceToMove, DWORD dwOrigin,
        ULARGE_INTEGER* lpNewFilePointer)
{ 
	int offset = 0;
	switch(dwOrigin)
	{
        case STREAM_SEEK_SET:
            offset = 0;
            break;
        case STREAM_SEEK_CUR:
			offset = m_curPos;
            break;
        case STREAM_SEEK_END:
			offset = m_size;
            break;
        default:   
            return STG_E_INVALIDFUNCTION;
            break;
        }
	offset += liDistanceToMove.LowPart;
	m_curPos = offset;
	if (lpNewFilePointer)
	{
		lpNewFilePointer->HighPart = 0;
		lpNewFilePointer->LowPart = m_curPos;
	}

//            return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

HRESULT STDMETHODCALLTYPE MemStream::Stat(STATSTG* pStatstg, DWORD grfStatFlag) 
{
	if (pStatstg)
	{
		pStatstg->cbSize.HighPart = 0;
		pStatstg->cbSize.LowPart = m_size;
	}
//	  if (GetFileSizeEx(_hFile, (PLARGE_INTEGER) &pStatstg->cbSize) == 0)
//            return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

