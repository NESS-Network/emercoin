#ifndef NAMECOIN_H
#define NAMECOIN_H

#include <hooks.h>
#include <rpc/protocol.h>
#include <wallet/db.h>
#include <script/interpreter.h>
#include <fs.h>
#include <index/txindex.h>
#include <script/standard.h>

class CWallet;
struct NameIndexStats;

static const unsigned int NAMEINDEX_CHAIN_SIZE = 1000;
static const int RELEASE_HEIGHT = 1<<16;

class CNameIndex
{
public:
    CDiskTxPos txPos;
    int nHeight;
    int op;
    CNameVal value;

    CNameIndex() : nHeight(0), op(0) {}

    CNameIndex(CDiskTxPos txPos, int nHeight, CNameVal value) :
        txPos(txPos), nHeight(nHeight), value(value) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(txPos);
        READWRITE(nHeight);
        READWRITE(op);
        READWRITE(value);
    }
};

// CNameRecord is all the data that is saved (in nameindexV3) with associated name
class CNameRecord
{
public:
    std::vector<CNameIndex> vtxPos;
    int nExpiresAt;
    int nLastActiveChainIndex;  // position in vtxPos of first tx in last active chain of name_new -> name_update -> name_update -> ....

    CNameRecord() : nExpiresAt(0), nLastActiveChainIndex(0) {}
    bool deleted()
    {
        if (!vtxPos.empty())
            return vtxPos.back().op == OP_NAME_DELETE;
        else return true;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vtxPos);
        READWRITE(nExpiresAt);
        READWRITE(nLastActiveChainIndex);
    }
};

// key = CNameVal, value = CNameRecord
class CNameDB : public CDBWrapper
{
public:
    CNameDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false) : CDBWrapper(GetDataDir() / "indexes" / "nameindexV3", nCacheSize, fMemory, fWipe) {
    }

    bool ReadName(const CNameVal& name, CNameRecord& rec) {
        bool ret = Read(name, rec);
        int s = rec.vtxPos.size();

         // check if array index is out of array bounds
        if (s > 0 && rec.nLastActiveChainIndex >= s) {
            LogPrintf("Nameindex is corrupt!");
        }
        return ret;
    }

    bool ScanNames(const CNameVal& name, unsigned int nMax,
            std::vector<
                std::pair<
                    CNameVal,
                    std::pair<CNameIndex, int>
                >
            > &nameScan);
    bool DumpToTextFile();
    bool GetNameIndexStats(NameIndexStats &stats);
};



// key = string, value = std::set<CNameVal>
// names listed here maybe expired
// names that have OP_NAME_DELETE as their last operation are not listed here

class CNameAddressDB : public CDBWrapper
{
public:
    CNameAddressDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false) : CDBWrapper(GetDataDir() / "indexes" / "nameaddressV3", nCacheSize, fMemory, fWipe) {
    }

    // this will fail if you try to add a name that already exist
    bool WriteSingleName(const std::string& address, const CNameVal& name) {
        std::set<CNameVal> names;
        Read(address, names); // note: address will not exist if this is the first time we are writting it
        return names.insert(name).second && Write(address, names);
    }

    // this will fail if you try to erase a name that does not exist
    bool EraseSingleName(const std::string& address, const CNameVal& name) {
        std::set<CNameVal> names;
        if (Read(address, names)) // note: address should always exist, because we are trying to erase name from existing record
            return names.erase(name) && Write(address, names);
        return false;
    }

    // removes name from old address and adds it to new address
    bool MoveName(const std::string& oldAddress, const std::string& newAddress, const CNameVal& name) {
        if (newAddress == oldAddress) // nothing to do
            return true;

        bool ret = true;
        if (oldAddress != "")
            ret = ret && EraseSingleName(oldAddress, name);
        if (newAddress != "")
            ret = ret && WriteSingleName(newAddress, name);
        return ret;
    }
    bool GetNameAddressIndexStats(NameIndexStats &stats);
};

extern std::map<CNameVal, std::set<uint256> > mapNamePending;
extern std::unique_ptr<CNameDB> pNameDB;
extern std::unique_ptr<CNameAddressDB> pNameAddressDB;

bool GetNameCurrentAddress(const CNameVal& name, CTxDestination& dest, std::string& error);
CNameVal nameValFromString(const std::string& str);
CNameVal toCNameVal(const std::string& str);
std::string stringFromNameVal(const CNameVal& nameVal);
std::string encodeNameVal(const CNameVal& input, const string& format);
std::string stringFromOp(int op);

CAmount GetNameOpFee(const CBlockIndex* pindexBlock, const int nRentalDays, int op, const CNameVal& name, const CNameVal& value);

void GetNameList(const CNameVal& nameUniq, std::map<CNameVal, NameTxInfo> &mapNames, std::map<CNameVal, NameTxInfo> &mapPending, CWallet* pwallet);
bool GetNameValue(const CNameVal& name, CNameVal& value);

struct NameTxReturn
{
     bool ok;
     std::string err_msg;
     RPCErrorCode err_code;
     std::string address;
     uint256 hex;   // Transaction hash in hex
};
NameTxReturn name_operation(const int op, const CNameVal& name, CNameVal value, const int nRentalDays, const string& strAddress, const string& strValueType, CWallet* pwallet);


struct nameTempProxy
{
    unsigned int nTime;
    CNameVal name;
    int op;
    uint256 hash;
    CNameIndex ind;
    std::string address;
    std::string prev_address;
};

#endif
