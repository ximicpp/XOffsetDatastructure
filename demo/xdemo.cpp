#include <iostream>
#include <fstream>
#include <filesystem>
#include <new>
#include <string>
#include <algorithm>
#include <chrono>
#include <numeric>
#include "xtypes.hpp"
#include "xtypeholder.hpp"

using namespace XOffsetDatastructure;

class TestType
{
public:
	template <typename Allocator>
	TestType(Allocator allocator) : mVector(allocator), mStringVector(allocator), mComplexMap(allocator), mStringSet(allocator), mSet(allocator), mString(allocator), TestTypeInnerObj(allocator), mXXTypeVector(allocator) {}
    // TestType(const TestType&) = delete;
    // TestType& operator=(const TestType&) = delete;
	int mInt{0};
	float mFloat{0.f};
	XVector<int> mVector;
	XVector<XString> mStringVector;
	class TestTypeInner
	{
	public:
		template <typename Allocator>
		TestTypeInner(Allocator allocator) : mVector(allocator) {}
		// TestTypeInner(const TestTypeInner&) = delete;
    	// TestTypeInner& operator=(const TestTypeInner&) = delete;
		int mInt{0};
		XVector<int> mVector;
	} TestTypeInnerObj;
	XVector<TestTypeInner> mXXTypeVector;
	XMap<XString, TestTypeInner> mComplexMap;
	XSet<XString> mStringSet;
	XSet<int> mSet;
	XString mString;
};

std::size_t writeData(const std::string &datafile, boost::container::vector<double> &writeTimes, bool writeFile = true)
{
	auto start = std::chrono::high_resolution_clock::now();
	XTypeHolder<TestType> holder;
	auto rootptr = holder.getOffsetPtr(); 
	// auto testtypedataptr = XTypeHolder<TestType>().getOffsetPtr();
	rootptr->mInt = 123;
	rootptr->mFloat = 3.14f;
	RETRY_IF_BAD_ALLOC(rootptr->mVector.push_back(1), holder);
	RETRY_IF_BAD_ALLOC(rootptr->mVector.push_back(3), holder);
	RETRY_IF_BAD_ALLOC(rootptr->mSet.insert(3), holder);
	RETRY_IF_BAD_ALLOC(rootptr->mSet.insert(4), holder);
	RETRY_IF_BAD_ALLOC(rootptr->mString = "abcdefghijklmnopqrstuvwxyz", holder);
	rootptr->TestTypeInnerObj.mInt = 246;
	RETRY_IF_BAD_ALLOC(rootptr->TestTypeInnerObj.mVector.push_back(23), holder);
	RETRY_IF_BAD_ALLOC(rootptr->TestTypeInnerObj.mVector.push_back(21), holder);
	for (auto i = 0; i < 1; ++i)
	{
		RETRY_IF_BAD_ALLOC(rootptr->mVector.push_back(i), holder);
		RETRY_IF_BAD_ALLOC(rootptr->TestTypeInnerObj.mVector.push_back(64 + i), holder);
	}

	for (auto i = 0; i < 6; ++i)
	{
		RETRY_IF_BAD_ALLOC(rootptr->mVector.push_back(1024 + i), holder);
		RETRY_IF_BAD_ALLOC(rootptr->mSet.insert(1024 - i), holder);
	}
	for (auto i = 0; i < 10; ++i)
	{
		RETRY_IF_BAD_ALLOC(rootptr->mStringVector.emplace_back("abcdefghijklmnopqrstuvwxyz", holder.getStorage()), holder);
	}
	// XTypeHolderOffsetPtr<XSet<int>> mSetPtr = holder.getOffsetPtr<XSet<int>>(reinterpret_cast<char*>(&(rootptr->mSet)));
	auto mSetPtr = holder.getOffsetPtr<XSet<int>>(rootptr->mSet);
	for (auto i = 0; i < 13; ++i)
	{
		RETRY_IF_BAD_ALLOC(rootptr->mStringSet.emplace("stringinset", holder.getStorage()), holder);
		RETRY_IF_BAD_ALLOC(mSetPtr->insert(i), holder);
	}

	for (auto i = 0; i < 7; ++i)
	{
		std::string key = "stringinset" + std::to_string(i);
		RETRY_IF_BAD_ALLOC(rootptr->mComplexMap.emplace(std::piecewise_construct,
																   std::forward_as_tuple(key.c_str(), holder.getStorage()),
																   std::forward_as_tuple(holder.getStorage())),
						   holder);
		auto vecPtr = holder.getOffsetPtr<XVector<int>>(rootptr->mComplexMap.find(key.c_str())->second.mVector);
		// for (auto j = 1; j < 256; ++j)
		// {
		// 	RETRY_IF_BAD_ALLOC(reinterpret_cast<XVector<int>*>(holder.getRawPointer(_offset))->emplace_back(j), holder);
		// 	// RETRY_IF_BAD_ALLOC(holder->mComplexMap.find(key.c_str())->second.mVector.emplace_back(j), holder);
		// }
		RETRY_IF_BAD_ALLOC((*vecPtr = {7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12, 7, 8, 9, 10, 11, 12,}), holder);
	}

	for (auto i = 0; i < 6; ++i)
	{
		RETRY_IF_BAD_ALLOC(rootptr->mXXTypeVector.emplace_back(holder.getStorage()), holder);
 		rootptr->mXXTypeVector.back().mInt = i;
		auto vecPtr = holder.getOffsetPtr<XVector<int>>(rootptr->mXXTypeVector.back().mVector);
		RETRY_IF_BAD_ALLOC((*vecPtr = {1, 2, 3, 4, 5, 6}), holder);
		for (auto j = 0; j < 12; ++j)
		{
			RETRY_IF_BAD_ALLOC((vecPtr->push_back(j)), holder);
		}
	}

	RETRY_IF_BAD_ALLOC(mSetPtr->insert(-1), holder);
	rootptr->mInt = 5;
	holder.trim();
	std::size_t storageSize = holder.getBuffer().size();

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	writeTimes.push_back(elapsed_seconds.count());

	if (writeFile)
	{
		std::ofstream ofs(datafile, std::ios::binary);
		ofs.write(reinterpret_cast<const char*>(holder.getBuffer().data()), holder.getBuffer().size());
		ofs.close();
	}
	return storageSize;
}

std::size_t readData(const std::string &datafile, boost::container::vector<double> &readTimes)
{
	std::ifstream ifs(datafile, std::ios::binary);
	ifs.seekg(0, std::ios::end);
	std::size_t storageSize = ifs.tellg();
	ifs.seekg(0, std::ios::beg);
	std::vector<std::byte> data2(storageSize);
	ifs.read(reinterpret_cast<char*>(data2.data()), storageSize);
	ifs.close();

	auto start = std::chrono::high_resolution_clock::now();
	XTypeHolder<TestType> holder(data2);
	auto rootptr = holder.getOffsetPtr(); 
	for (auto i = 0; i < 12; ++i)
	{
		RETRY_IF_BAD_ALLOC((rootptr->mVector.push_back(1024 + i)), holder);
		RETRY_IF_BAD_ALLOC((rootptr->mSet.insert(1024 - i)), holder);
	}
	std::cout << rootptr->mInt << std::endl;
	std::cout << rootptr->mFloat << std::endl;
	for (auto i : rootptr->mVector)
	{
		std::cout << i << " ";
	}
	std::cout << std::endl;
	for (auto &i : rootptr->mStringVector)
	{
		std::cout << i << " ";
	}
	std::cout << std::endl;
	// auto vs auto&
	for (auto &i : rootptr->mComplexMap)
	{
		std::cout << i.first << ": " << i.second.mInt << " ";
		for (auto j : i.second.mVector)
		{
			std::cout << j << " ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
	for (auto &i : rootptr->mSet)
	{
		std::cout << i << " ";
	}
	std::cout << std::endl;
	std::cout << rootptr->mString << std::endl;
	for (auto i : rootptr->TestTypeInnerObj.mVector)
	{
		std::cout << i << " ";
	}
	std::cout << std::endl;
	std::cout << rootptr->TestTypeInnerObj.mInt << std::endl;
	for (auto &i : rootptr->mXXTypeVector)
	{
		std::cout << i.mInt << std::endl;
		for (auto j : i.mVector)
		{
			std::cout << j << " ";
		}
		std::cout << std::endl;
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	readTimes.push_back(elapsed_seconds.count());

	return storageSize;
}

int main()
{
	bool forceRemove = false;
	const std::string datafile("data.bin");
	const int numRuns = 0;
	std::size_t storageSize = 0;
	boost::container::vector<double> writeTimes(numRuns);
	boost::container::vector<double> readTimes(numRuns);
	if (forceRemove)
	{
		std::filesystem::remove(datafile);
	}
	if (!std::filesystem::exists(datafile))
	{
		storageSize = writeData(datafile, writeTimes);
	}
	readData(datafile, readTimes);
	for (int i = 0; i < numRuns; ++i)
	{
		storageSize = writeData(datafile, writeTimes, false);
		readData(datafile, readTimes);
	}
	double writeTimeAvg = std::accumulate(writeTimes.begin(), writeTimes.end(), 0.0) / numRuns;
	double readTimeAvg = std::accumulate(readTimes.begin(), readTimes.end(), 0.0) / numRuns;
	std::cout << std::left << std::setw(25) << "Average writeData time:" << std::right << std::setw(10) << std::fixed << std::setprecision(3) << writeTimeAvg * 1000 << " ms" << std::endl;
	std::cout << std::left << std::setw(25) << "Average readData time:" << std::right << std::setw(10) << std::fixed << std::setprecision(3) << readTimeAvg * 1000 << " ms" << std::endl;
	std::cout << "storageSize: " << storageSize << std::endl;
	return 0;
}