#pragma warning(disable : 4101 4244 4267 4566)

// pch.h (updated global state transition)

#pragma once

#include <fstream>
#include <string>
#include <map>
#include <vector>

// Type definition for good
using GoodType = long;
const GoodType UndefGoodType = -999;
const GoodType CentralBankType = -2; // Special type for Central Bank
const GoodType CommercialBankType = -3; // Special type for Commercial Bank

// Type definition for quantity
using GoodQtty = long long;

namespace Region {

class CGoods; // Forward declaration

// Translation maps and vectors 
static std::vector<std::string> indexToName;       // Index by GoodType, get name
static std::vector<std::string> indexToLabel;      // Index by GoodType, get SAM label
static std::map<std::string, GoodType> nameToIndex; // Map from name to GoodType
static std::map<std::string, GoodType> labelToIndex; // Map from SAM label to GoodType

const GoodType undefinedGoodType = -1;

//======================  CGoods  ===================================

class CGoods : public std::vector<GoodQtty>
{
private:
	static std::vector<std::string> _GoodTypeToName;
	static std::map<std::string, GoodType> _GoodNameToType;

public:
	CGoods();
	explicit CGoods(size_t numTypes);
	~CGoods();

	static void clearGoodsDefinitions();
	static void defineNewGoodType(const std::string& name);
	static size_t getNTypes();

	static std::vector<std::string>& vGoodTypeToName();
	static const std::vector<std::string>& GoodTypeToName();
	static const std::string& GoodTypeToName(GoodType gTy);
	static std::map<std::string, GoodType>& mGoodNameToType();
	static const std::map<std::string, GoodType>& GoodNameToType();
	static GoodType GoodNameToType(const std::string& gname);

	GoodQtty& operator[](GoodType gType);
	GoodQtty operator()(GoodType gType) const;
	CGoods& operator=(const CGoods& cgds);
	CGoods operator+(const CGoods& cgds) const;
	CGoods& operator+=(const CGoods& cgds);
	CGoods operator-(const CGoods& cgds) const;
	CGoods& operator-=(const CGoods& cgds);

	CGoods operator*(double scalar) const;
	CGoods& operator*=(double scalar);
	CGoods operator/(double scalar) const;
	CGoods& operator/=(double scalar);

	friend std::ofstream& operator<<(std::ofstream& ofstrm, const CGoods& cgoods);
	friend std::ifstream& operator>>(std::ifstream& ifstrm, CGoods& goods);
};

std::ofstream& operator<<(std::ofstream& ofstrm, const CGoods& cgoods);
std::ifstream& operator>>(std::ifstream& ifstrm, CGoods& goods);

} // namespace Region
