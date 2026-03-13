#include <stdexcept>      // For std::out_of_range, std::runtime_error
#include <cassert>        // For assert
#include <numeric>        // For std::accumulate (if needed for sums)
#include <algorithm>      // For std::transform, std::fill
#include "Goods.h" // Or your main project header if pchDeployers3.h is included there
#include "../DataManager/DataManager.h"

namespace Region {


	// Initialize static members of CGoods
	std::vector<std::string> CGoods::_GoodTypeToName;
	std::map<std::string, GoodType> CGoods::_GoodNameToType;

	//==================   CGoods Implementation  ===========================

	// --- Static methods ---
	void CGoods::clearGoodsDefinitions() {
		_GoodTypeToName.clear();
		_GoodNameToType.clear();
		// Add predefined types if they are not meant to be cleared or re-derived from SAM
		// For now, assuming SAM-derived types are primary and banks are special additions.
	}

	void CGoods::defineNewGoodType(const std::string& gName) {
		if (_GoodNameToType.find(gName) == _GoodNameToType.end()) {
			GoodType newType = static_cast<GoodType>(_GoodTypeToName.size());
			_GoodNameToType[gName] = newType;
			_GoodTypeToName.push_back(gName);
		}
		// If type already exists, do nothing or log a warning.
	}

	size_t CGoods::getNTypes() {
		// This should reflect the number of SAM-defined producer types.
		// Bank types are special and might not be included in this count
		// if this count is strictly for SAM goods/producer sectors.
		// If CGoods is used to size vectors for all agent types including banks,
		// this might need adjustment or a different way to get total types.
		// For now, assume it's for SAM producer types.
		return DataManager::GetCurrentSAM()->getnPproducerTypes();
	}

	std::vector<std::string>& CGoods::vGoodTypeToName() {
		return _GoodTypeToName;
	}

	const std::vector<std::string>& CGoods::GoodTypeToName() {
		return _GoodTypeToName;
	}

	const std::string& CGoods::GoodTypeToName(GoodType gTy) {
		if (gTy == CentralBankType) {
			static const std::string cbName = "CentralBank";
			return cbName;
		}
		if (gTy == CommercialBankType) {
			static const std::string comBName = "CommercialBank";
			return comBName;
		}
		if (gTy >= 0 && static_cast<size_t>(gTy) < _GoodTypeToName.size()) {
			return _GoodTypeToName[static_cast<size_t>(gTy)];
		}
		static const std::string unknown = "UnknownGoodType";
		// Consider throwing an error for invalid gTy
		// assert(false && "Invalid GoodType for GoodTypeToName");
		return unknown;
	}

	std::map<std::string, GoodType>& CGoods::mGoodNameToType() {
		return _GoodNameToType;
	}

	const std::map<std::string, GoodType>& CGoods::GoodNameToType() {
		return _GoodNameToType;
	}

	GoodType CGoods::GoodNameToType(const std::string& gname) {
		if (gname == "CentralBank") {
			return CentralBankType;
		}
		if (gname == "CommercialBank") {
			return CommercialBankType;
		}
		auto it = _GoodNameToType.find(gname);
		if (it != _GoodNameToType.end()) {
			return it->second;
		}
		// Consider throwing an error for unknown gname
		// assert(false && "Unknown good name for GoodNameToType");
		return UndefGoodType; // Or throw std::out_of_range("Unknown good name: " + gname);
	}

	// --- Constructors ---
	CGoods::CGoods() : std::vector<GoodQtty>(DataManager::GetCurrentSAM()->getnPproducerTypes(), 0) {
		// Default constructor initializes with current nPproducerTypes, all quantities to 0.
		// nPproducerTypes should be set by SAM before CGoods objects are created.
	}

	CGoods::CGoods(size_t numTypes) : std::vector<GoodQtty>(numTypes, 0) {
		// Constructor to explicitly size the vector.
		// This could be used if nPproducerTypes is not yet globally set or for specific CGoods instances.
		auto nPproducerTypes = DataManager::GetCurrentSAM()->getnPproducerTypes();
		if (nPproducerTypes == 0 && numTypes > 0) {
			// Potentially update global nPproducerTypes if this is the first CGoods with size.
			// This logic depends on how nPproducerTypes is intended to be managed.
			// For now, let's assume nPproducerTypes is set separately by the SAM.
		}
		else if (nPproducerTypes > 0 && numTypes != nPproducerTypes) {
			// Log warning or error if constructing with a size different from global nPproducerTypes
			// assert(false && "CGoods constructed with size different from global nPproducerTypes");
		}
	}


	CGoods::~CGoods() = default;

	// --- Accessors ---
	GoodQtty& CGoods::operator[](GoodType gType) {
		auto nPproducerTypes = DataManager::GetCurrentSAM()->getnPproducerTypes();
		if (static_cast<size_t>(gType) >= this->size()) {
			// This case should ideally not happen if CGoods is always sized to nPproducerTypes
			// and gType is always valid.
			// If it can happen, resize or throw error.
			// For now, assume gType is a valid index.
			// Resize if gType is valid for global nPproducerTypes but vector is smaller (e.g. default constructed before nPproducerTypes set)
			if (static_cast<size_t>(gType) >= nPproducerTypes) {
				throw std::out_of_range("CGoods::operator[]: GoodType out of range for nPproducerTypes.");
			}
			if (this->size() < nPproducerTypes) {
				this->resize(nPproducerTypes, 0);
			}
		}
		// Ensure gType is within the valid range of defined good types
		if (gType < 0 || static_cast<size_t>(gType) >= nPproducerTypes) {
			throw std::out_of_range("CGoods::operator[]: GoodType is invalid.");
		}
		return this->at(static_cast<size_t>(gType));
	}

	GoodQtty CGoods::operator()(GoodType gType) const {
		auto nPproducerTypes = DataManager::GetCurrentSAM()->getnPproducerTypes();
		if (static_cast<size_t>(gType) >= this->size()) {
			// If gType is valid for global nPproducerTypes but vector is smaller
			if (static_cast<size_t>(gType) < nPproducerTypes) return 0; // Treat as 0 if not explicitly stored yet but valid type
			throw std::out_of_range("CGoods::operator(): GoodType out of range for nPproducerTypes.");
		}
		if (gType < 0 || static_cast<size_t>(gType) >= nPproducerTypes) {
			throw std::out_of_range("CGoods::operator(): GoodType is invalid.");
		}
		return this->at(static_cast<size_t>(gType));
	}

	// --- Operators for CGoods ---
	CGoods& CGoods::operator=(const CGoods& cgds) {
		if (this != &cgds) {
			// std::vector assignment operator handles this well.
			// Ensure sizes are compatible or handle resizing.
			// If nPproducerTypes is fixed, both should have the same size.
			if (this->size() != cgds.size()) {
				// This might indicate an issue if nPproducerTypes is supposed to be consistent.
				// assert(false && "CGoods::operator=: Assigning CGoods of different sizes.");
				// For safety, adopt the size of cgds, or resize to nPproducerTypes.
				// Let's stick to standard vector assignment behavior.
			}
			static_cast<std::vector<GoodQtty>&>(*this) = static_cast<const std::vector<GoodQtty>&>(cgds);
		}
		return *this;
	}

	CGoods CGoods::operator+(const CGoods& cgds) const {
		if (this->size() != cgds.size()) {
			// assert(false && "CGoods::operator+: Adding CGoods of different sizes.");
			throw std::runtime_error("CGoods::operator+: Adding CGoods of different sizes.");
		}
		CGoods result(this->size()); // Or CGoods result(nPproducerTypes);
		std::transform(this->begin(), this->end(), cgds.begin(), result.begin(), std::plus<GoodQtty>());
		return result;
	}

	CGoods& CGoods::operator+=(const CGoods& cgds) {
		if (this->size() != cgds.size()) {
			// assert(false && "CGoods::operator+=: Adding CGoods of different sizes.");
			throw std::runtime_error("CGoods::operator+=: Adding CGoods of different sizes.");
		}
		std::transform(this->begin(), this->end(), cgds.begin(), this->begin(), std::plus<GoodQtty>());
		return *this;
	}

	CGoods CGoods::operator-(const CGoods& cgds) const {
		if (this->size() != cgds.size()) {
			// assert(false && "CGoods::operator-: Subtracting CGoods of different sizes.");
			throw std::runtime_error("CGoods::operator-: Subtracting CGoods of different sizes.");
		}
		CGoods result(this->size()); // Or CGoods result(nPproducerTypes);
		std::transform(this->begin(), this->end(), cgds.begin(), result.begin(), std::minus<GoodQtty>());
		return result;
	}

	CGoods& CGoods::operator-=(const CGoods& cgds) {
		if (this->size() != cgds.size()) {
			// assert(false && "CGoods::operator-=: Subtracting CGoods of different sizes.");
			throw std::runtime_error("CGoods::operator-=: Subtracting CGoods of different sizes.");
		}
		std::transform(this->begin(), this->end(), cgds.begin(), this->begin(), std::minus<GoodQtty>());
		return *this;
	}

	CGoods CGoods::operator*(double scalar) const {
		CGoods result(this->size());
		std::transform(this->begin(), this->end(), result.begin(),
			[scalar](GoodQtty val) { return static_cast<GoodQtty>(static_cast<double>(val) * scalar); });
		return result;
	}

	CGoods& CGoods::operator*=(double scalar) {
		std::transform(this->begin(), this->end(), this->begin(),
			[scalar](GoodQtty val) { return static_cast<GoodQtty>(static_cast<double>(val) * scalar); });
		return *this;
	}

	CGoods CGoods::operator/(double scalar) const {
		if (scalar == 0.0) {
			throw std::runtime_error("CGoods::operator/: Division by zero.");
		}
		CGoods result(this->size());
		std::transform(this->begin(), this->end(), result.begin(),
			[scalar](GoodQtty val) { return static_cast<GoodQtty>(static_cast<double>(val) / scalar); });
		return result;
	}

	CGoods& CGoods::operator/=(double scalar) {
		if (scalar == 0.0) {
			throw std::runtime_error("CGoods::operator/=: Division by zero.");
		}
		std::transform(this->begin(), this->end(), this->begin(),
			[scalar](GoodQtty val) { return static_cast<GoodQtty>(static_cast<double>(val) / scalar); });
		return *this;
	}

	// Stream operators for CGoods
	std::ofstream& operator<<(std::ofstream& ofstrm, const CGoods& cgoods) {
		ofstrm << "{";
		for (GoodType gType = 0; static_cast<size_t>(gType) < cgoods.size(); ++gType) {
			// Only print if quantity is non-zero, or always print?
			// The old map version would only iterate over existing keys.
			// For vector, we iterate all nPproducerTypes. Let's print if non-zero to be less verbose.
			// Or, to be consistent with potential loading, print all.
			// For now, let's print all for easier debugging and consistent structure.
			if (static_cast<size_t>(gType) < CGoods::GoodTypeToName().size()) { // Check if name exists
				ofstrm << " " << CGoods::GoodTypeToName(gType) << " " << cgoods(gType);
			}
			else {
				// This case should ideally not happen if nPproducerTypes and _GoodTypeToName are synced.
				ofstrm << " UnknownGoodTypeIndex(" << gType << ") " << cgoods(gType);
			}
		}
		ofstrm << " }";
		return ofstrm;
	}

	std::ifstream& operator>>(std::ifstream& ifstrm, CGoods& goods) {
		// Ensure goods vector is sized correctly before filling.
		// If nPproducerTypes is set, goods should already be sized by its constructor.
		// If not, this is problematic. Assume goods is already sized to nPproducerTypes.
		if (goods.size() != CGoods::getNTypes() && CGoods::getNTypes() > 0) {
			goods.resize(CGoods::getNTypes(), 0); // Ensure it's sized
		}
		else if (CGoods::getNTypes() == 0 && goods.empty()) {
			// Cannot proceed if nPproducerTypes is unknown and goods is empty.
			// This indicates an initialization order problem.
			throw std::runtime_error("CGoods input stream: nPproducerTypes not set and goods vector is empty.");
		}


		std::string name, bracket;
		GoodQtty qtty;
		ifstrm >> bracket; // Read "{"
		if (bracket != "{") {
			ifstrm.setstate(std::ios_base::failbit); // Set failbit if format is wrong
			return ifstrm;
		}

		// Clear the vector before populating from stream, or fill based on names.
		// Since it's a vector of fixed size nPproducerTypes, we should fill based on names.
		std::fill(goods.begin(), goods.end(), 0); // Initialize all to 0

		while (ifstrm.good()) {
			ifstrm >> name;
			if (name == "}") break; // End of CGoods entry

			if (ifstrm >> qtty) {
				GoodType gType = CGoods::GoodNameToType(name);
				if (gType != UndefGoodType && static_cast<size_t>(gType) < goods.size()) {
					goods[gType] = qtty;
				}
				else {
					// Unknown good name or type out of bounds for this CGoods instance
					// Log error or set failbit
					// For now, just skip unknown goods in the stream.
					// ifstrm.setstate(std::ios_base::failbit);
					// return ifstrm;
				}
			}
			else {
				// Failed to read quantity, likely format error or EOF
				if (name != "}") { // If it wasn't the closing bracket
					ifstrm.setstate(std::ios_base::failbit);
				}
				break;
			}
		}
		if (name != "}") { // If loop exited without finding closing bracket
			ifstrm.setstate(std::ios_base::failbit);
		}
		return ifstrm;
	}
} // namespace Region