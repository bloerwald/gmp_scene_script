struct DB2Header
{
  uint32_t magic;                                               // 'WDB6' for .db2 (database)
  uint32_t record_count;
  uint32_t field_count;                                         // for the first time, this counts arrays as '1'; in the past, only the WCH* variants have counted arrays as 1 field
  uint32_t record_size;
  uint32_t string_table_size;                                   // if flags & 0x01 != 0, this field takes on a new meaning - it becomes an absolute offset to the beginning of the offset_map
  uint32_t table_hash;
  uint32_t layout_hash;                                         // used to be 'build', but after build 21737, this is a new hash field that changes only when the structure of the data changes
  uint32_t min_id;
  uint32_t max_id;
  uint32_t locale;                                              // as seen in TextWowEnum
  uint32_t copy_table_size;
  uint16_t flags;                                               // in WDB3/WCH4, this field was in the WoW executable's DBCMeta; possible values are listed in Known Flag Meanings
  uint16_t id_index;                                            // new in WDB5 (and only after build 21737), this is the index of the field containing ID values; this is ignored if flags & 0x04 != 0
  uint32_t total_field_count;                                   // new in WDB6, includes columns only expressed in the 'common_data_table', unlike field_count
  uint32_t common_data_table_size;                              // new in WDB6, size of new block called 'common_data_table'
};

static_assert (sizeof (DB2Header) == 0x38, "size of DB2Header");

namespace
{
  uint32_t emplace_string (std::vector<char>& stringblock, std::string str)
  {
    uint32_t pos (stringblock.size());
    for (char const& c : str)
    {
      stringblock.emplace_back (c);
    }
    stringblock.emplace_back ('\0');
    return pos;
  }
}

struct SceneScriptPackageRecRaw;
struct SceneScriptPackageRec
{
  int id;
  std::string name;
  SceneScriptPackageRec clone (int newid) const
  {
    auto x (*this);
    x.id = newid;
    return x;
  }
  SceneScriptPackageRecRaw raw (std::vector<char>& stringblock) const;
};

struct SceneScriptPackageRecRaw
{
  unsigned int name;
  static const unsigned int table_hash = 0xE8CB5E09;
  static constexpr uint32_t const layout_hash = 956619678;
  static constexpr uint32_t const field_count = 2;
  static constexpr std::array<uint16_t, 2> field_layout = {0, 0};
  SceneScriptPackageRec unraw (DB2Header const& header, const char* stringblock, int id) const
  {
    SceneScriptPackageRec rec;
    rec.id = id;
    rec.name = name + stringblock;
    return rec;
  }
};
static_assert (sizeof (SceneScriptPackageRecRaw) == 0x4, "size of SceneScriptPackageRecRaw");

SceneScriptPackageRecRaw SceneScriptPackageRec::raw (std::vector<char>& stringblock) const
{
  SceneScriptPackageRecRaw rec;
  rec.name = emplace_string (stringblock, name);
  return rec;
}


struct SceneScriptPackageMemberRecRaw;
struct SceneScriptPackageMemberRec
{
  int id;
  int package;
  int script;
  int d;
  int sequence;
  SceneScriptPackageMemberRec clone (int newid) const
  {
    auto x (*this);
    x.id = newid;
    return x;
  }
  SceneScriptPackageMemberRecRaw raw (std::vector<char>& stringblock) const;
};

struct SceneScriptPackageMemberRecRaw
{
  uint16_t package;
  uint16_t script;
  uint16_t d;
  uint16_t sequence;
  static const unsigned int table_hash = 0xE44DB71C;
  static constexpr uint32_t const layout_hash = 275693289;
  static constexpr uint32_t const field_count = 5;
  static constexpr std::array<uint16_t, 8> field_layout = {0x10, 0, 0x10, 2, 0x10, 4, 0x18, 6};
  SceneScriptPackageMemberRec unraw (DB2Header const& header, const char* stringblock, int id) const
  {
    SceneScriptPackageMemberRec rec;
    rec.id = id;
    rec.package = package;
    rec.script = script;
    rec.sequence = sequence;
    rec.d = d;
    return rec;
  }
};
static_assert (sizeof (SceneScriptPackageMemberRecRaw) == 0x8, "size of SceneScriptPackageMemberRec");

SceneScriptPackageMemberRecRaw SceneScriptPackageMemberRec::raw (std::vector<char>& stringblock) const
{
  SceneScriptPackageMemberRecRaw rec;
  rec.package = package;
  rec.script = script;
  rec.d = d;
  rec.sequence = sequence;
  return rec;
}


struct SceneScriptRecRaw;
struct SceneScriptRec
{
  int id;
  std::string name;
  std::string content;
  int previous_script;
  int next_script;
  SceneScriptRec clone (int newid) const
  {
    auto x (*this);
    x.id = newid;
    return x;
  }
  SceneScriptRecRaw raw (std::vector<char>& stringblock) const;
};

struct SceneScriptRecRaw
{
  uint32_t name;
  uint32_t content;
  uint16_t previous_script;
  uint16_t next_script;
  static const unsigned int table_hash = 0xD4B163CC;
  static constexpr uint32_t const layout_hash = 1240380216;
  static constexpr uint32_t const field_count = 5;
  static constexpr std::array<uint16_t, 8> const field_layout = {0, 0, 0, 4, 0x10, 8, 0x10, 0xa};
  SceneScriptRec unraw (DB2Header const& header, const char* stringblock, int id) const
  {
    SceneScriptRec rec;
    rec.id = id;
    rec.name = name + stringblock;
    rec.content = content + stringblock;
    rec.next_script = next_script;
    rec.previous_script = previous_script;
    return rec;
  }
};
static_assert (sizeof (SceneScriptRecRaw) == 0xc, "size of SceneScriptRecRaw");

SceneScriptRecRaw SceneScriptRec::raw (std::vector<char>& stringblock) const
{
  SceneScriptRecRaw rec;
  rec.name = emplace_string (stringblock, name);
  rec.content = emplace_string (stringblock, content);
  rec.previous_script = previous_script;
  rec.next_script = next_script;
  return rec;
}

constexpr std::array<uint16_t, 8> const SceneScriptRecRaw::field_layout;
constexpr std::array<uint16_t, 2> const SceneScriptPackageRecRaw::field_layout;
constexpr std::array<uint16_t, 8> const SceneScriptPackageMemberRecRaw::field_layout;
