//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include <wrl.h>
#include <ppl.h>
#include <ppltasks.h>

namespace DX
{
	struct ByteArray { Platform::Array<byte>^ data; };

	// function that reads from a binary file asynchronously
	inline Concurrency::task<ByteArray> ReadDataAsync(Platform::String^ filename)
	{
		using namespace Windows::Storage;
		using namespace Concurrency;
		
		auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;
		
		task<StorageFile^> getFileTask(folder->GetFileAsync(filename));

#ifdef _WINPHONE
		// Create a local to allow the DataReader to be passed between lambdas.
		auto reader = std::make_shared<Streams::DataReader^>(nullptr);
		return getFileTask.then([](StorageFile^ file)
		{
			return file->OpenReadAsync();
		}).then([reader](Streams::IRandomAccessStreamWithContentType^ stream)
		{
			*reader = ref new Streams::DataReader(stream);
			return (*reader)->LoadAsync(static_cast<uint32>(stream->Size));
		}).then([reader](uint32 count)
		{
			auto a = ref new Platform::Array<byte>(count);
			(*reader)->ReadBytes(a);
			ByteArray ba = { a };
			return ba;
		});
#else
		auto readBufferTask = getFileTask.then([] (StorageFile^ f) 
		{
			return FileIO::ReadBufferAsync(f);
		});

		auto byteArrayTask = readBufferTask.then([] (Streams::IBuffer^ b) -> ByteArray 
		{
			auto a = ref new Platform::Array<byte>(b->Length);
			Streams::DataReader::FromBuffer(b)->ReadBytes(a);
			ByteArray ba = { a };
			return ba;
		});
		return byteArrayTask;
#endif
	}
}