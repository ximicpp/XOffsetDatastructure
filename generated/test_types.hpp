#ifndef GENERATED_TEST_TYPES_HPP_
#define GENERATED_TEST_TYPES_HPP_

#include "../xoffsetdatastructure2.hpp"
#include "XTypeSignature.hpp"

using namespace XOffsetDatastructure2;

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestTypeInner {
	template <typename Allocator>
	TestTypeInner(Allocator allocator) : mVector(allocator) {}
	int mInt{0};
	XVector<int> mVector;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestType {
	template <typename Allocator>
	TestType(Allocator allocator) : mVector(allocator), mStringVector(allocator), TestTypeInnerObj(allocator), mXXTypeVector(allocator), mComplexMap(allocator), mStringSet(allocator), mSet(allocator), mString(allocator) {}
	int mInt{0};
	float mFloat{0.0f};
	XVector<int> mVector;
	XVector<XString> mStringVector;
	TestTypeInner TestTypeInnerObj;
	XVector<TestTypeInner> mXXTypeVector;
	XMap<XString, TestTypeInner> mComplexMap;
	XSet<XString> mStringSet;
	XSet<int> mSet;
	XString mString;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestTypeInnerReflectionHint {
	int32_t mInt;
	XVector<int32_t> mVector;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestTypeReflectionHint {
	int32_t mInt;
	float mFloat;
	XVector<int32_t> mVector;
	XVector<XString> mStringVector;
	TestTypeInnerReflectionHint TestTypeInnerObj;
	XVector<TestTypeInnerReflectionHint> mXXTypeVector;
	XMap<XString, TestTypeInnerReflectionHint> mComplexMap;
	XSet<XString> mStringSet;
	XSet<int32_t> mSet;
	XString mString;
};

#endif // GENERATED_TEST_TYPES_HPP_
