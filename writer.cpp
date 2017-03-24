#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

struct DB2Header
{
  int magic;
  int record_count;
  int field_count;
  int record_size;
  int string_table_size;
  unsigned int table_hash;
  int build;
  int unk1;
  int min_id;
  int max_id;
  int locale;
  int unk2;
};
static_assert (sizeof (DB2Header) == 0x30, "size of DB2Header");

struct SceneScriptPackageRec
{
  int id;
  const char* name;
};
struct SceneScriptPackageRecRaw
{
  int id;
  unsigned int name;
  static const unsigned int table_hash = 0xE8CB5E09;
  SceneScriptPackageRec unraw (DB2Header const& header, const char* stringblock) const
  {
    SceneScriptPackageRec rec;
    rec.id = id;
    rec.name = name + stringblock;
    return rec;
  }
};
static_assert (sizeof (SceneScriptPackageRecRaw) == 0x8, "size of SceneScriptPackageRecRaw");

struct SceneScriptPackageMemberRec
{
  int id;
  int package;
  int script;
  int sequence;
  int d;
};
struct SceneScriptPackageMemberRecRaw
{
  int id;
  int package;
  int script;
  int sequence;
  int d;
  static const unsigned int table_hash = 0xE44DB71C;
  SceneScriptPackageMemberRec unraw (DB2Header const& header, const char* stringblock) const
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
static_assert (sizeof (SceneScriptPackageMemberRec) == 0x14, "size of SceneScriptPackageMemberRec");

struct SceneScriptRec
{
  int id;
  const char* name;
  const char* content;
  int previous_script;
  int next_script;
};
struct SceneScriptRecRaw
{
  int id;
  unsigned int name;
  unsigned int content;
  int previous_script;
  int next_script;
  static const unsigned int table_hash = 0xD4B163CC;
  SceneScriptRec unraw (DB2Header const& header, const char* stringblock) const
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
static_assert (sizeof (SceneScriptRecRaw) == 0x14, "size of SceneScriptRecRaw");

std::vector<char> read_file (const std::string filename)
{
  std::vector<char> buffer;
  if (FILE* fp = fopen (filename.c_str(), "r"))
  {
    char buf[1024];
    while (size_t len = fread (buf, 1, sizeof(buf), fp))
    {
      buffer.insert (buffer.end(), buf, buf + len);
    }
    fclose (fp);
  }
  else
  {
    throw std::runtime_error ("file not opened: " + filename);
  }
  return buffer;
}
void write_file (const boost::filesystem::path filename, std::vector<char> data)
{
  if (FILE* fp = fopen (filename.string().c_str(), "w"))
  {
    fwrite (data.data(), data.size(), 1, fp);
    fclose (fp);
  }
  else
  {
    throw std::runtime_error ("file not opened: " + filename.string());
  }
}

template<typename Rec>
std::vector<char> put_records (std::vector<Rec> const& recs, std::vector<char> const& strings)
{
  std::vector<char> data (sizeof (DB2Header) + sizeof (Rec) * recs.size());
  data.insert (data.end(), strings.begin(), strings.end());

  DB2Header* header (reinterpret_cast<DB2Header*> (data.data()));

  header->magic = '2BDW';
  header->record_count = recs.size();
  header->field_count = sizeof (Rec) / 4;
  static_assert (sizeof (Rec) % 4 == 0, "assume all-4byte-fields");
  header->record_size = sizeof (Rec);
  header->string_table_size = strings.size();
  header->table_hash = Rec::table_hash;
  header->build = 17898;
  header->unk1 = 0;
  header->min_id = 0;
  header->max_id = 0;
  header->locale = -1;
  header->unk2 = 0;

  std::copy ( (char*)recs.data(), (char*) (recs.data() + recs.size())
            , data.data() + sizeof (DB2Header)
            );

  return data;
}

std::string replace_not_permitted_characters (std::string str)
{
  std::replace (str.begin(), str.end(), '/', ',');
  return str;
}

struct package_member_t
{
  std::string name;
  std::string content;
  int include_id;
};

struct package_t
{
  std::string name;
  std::map<int, package_member_t> members;
};

int main (int argc, char** argv)
{
  const std::string scene_script_filename
    ("DBFilesClient/SceneScript.db2");
  const std::string scene_script_package_filename
    ("DBFilesClient/SceneScriptPackage.db2");
  const std::string scene_script_package_member_filename
    ("DBFilesClient/SceneScriptPackageMember.db2");

  const boost::filesystem::path input_dir (boost::filesystem::current_path() / "scene_scripts");
  const boost::filesystem::path output_dir ("/Applications/World of Warcraft Public Test");
  const boost::filesystem::path by_id_dir (input_dir / "by id");

  boost::filesystem::create_directories (output_dir / "DBFilesClient");
  // boost::filesystem::create_directories (by_name_dir);

  std::map<int, package_t> packages;
  std::map<int, int> packages_raw;

  for ( boost::filesystem::directory_entry package_dentry
      : boost::make_iterator_range (boost::filesystem::directory_iterator (by_id_dir), boost::filesystem::directory_iterator())
      )
  {
    const boost::filesystem::path package_dir (package_dentry.path());

    if (package_dir.stem().string().empty())
    {
      continue;
    }

    std::ifstream package_name_stream ((package_dir / "name.txt").string());
    std::string package_name;
    std::getline (package_name_stream, package_name);

    package_t package;
    package.name = package_name;

    for ( boost::filesystem::directory_entry member_dentry
        : boost::make_iterator_range (boost::filesystem::directory_iterator (package_dir), boost::filesystem::directory_iterator())
        )
    {
      const boost::filesystem::path member_path (member_dentry.path());

      if (member_path.stem().string().empty())
      {
        continue;
      }

      package_member_t package_member;
      const std::string member_path_str (member_path.stem().string());
      package_member.name = { member_path_str.begin() + member_path_str.find_first_of ('.') + 1
                            , member_path_str.end()
                            };
      package_member.include_id = 0;

      if (member_path.extension() == ".inc")
      {
        package_member.include_id =
          std::stoi (boost::filesystem::read_symlink (member_path).stem().string());
      }
      else if (member_path.extension() == ".lua")
      {
        const std::vector<char> content_raw (read_file (member_path.string()));
        package_member.content = {content_raw.begin(), content_raw.end()};
      }
      else
      {
        continue;
      }

      package.members.emplace
        ( std::stoi (std::string ( member_path_str.begin()
                                 , member_path_str.begin() + member_path_str.find_first_of ('.')
                                 )
                    )
        , package_member
        );
    }

    packages.emplace (std::stoi (package_dir.stem().string()), package);
  }

  std::vector<SceneScriptPackageRecRaw> package_recs;
  std::vector<char> package_strings;
  std::vector<SceneScriptPackageMemberRecRaw> package_member_recs;
  std::vector<char> package_member_strings;
  std::vector<SceneScriptRecRaw> script_recs;
  std::vector<char> script_strings;

  package_strings.push_back ('\0');
  package_member_strings.push_back ('\0');
  script_strings.push_back ('\0');

  int package_member_id (1);
  int script_id (1);

  for (std::pair<int, package_t> package : packages)
  {
    SceneScriptPackageRecRaw package_rec;
    package_rec.id = package.first;
    package_rec.name = package_strings.size();
    package_strings.insert
      (package_strings.end(), package.second.name.begin(), package.second.name.end());
    package_strings.push_back ('\0');

    for (std::pair<int, package_member_t> member : package.second.members)
    {
      SceneScriptPackageMemberRecRaw package_member_rec;
      package_member_rec.id = package_member_id;
      package_member_rec.package = package_rec.id;
      package_member_rec.script = 0;
      package_member_rec.sequence = member.first;
      package_member_rec.d = member.second.include_id;

      if (!member.second.include_id)
      {
        package_member_rec.script = script_id;
        constexpr int per_content_part (4000);
        const int content_parts
          (std::max (1UL, (member.second.content.size() + per_content_part - 1) / per_content_part));

        for (int content_part (0); content_part < content_parts; ++content_part)
        {
          const std::string part_of_content
            (member.second.content.substr (content_part * per_content_part, per_content_part));

          SceneScriptRecRaw script_rec;
          script_rec.id = script_id;
          script_rec.name = script_strings.size();
          script_strings.insert
            (script_strings.end(), member.second.name.begin(), member.second.name.end());
          script_strings.push_back ('\0');
          script_rec.content = script_strings.size();
          script_strings.insert
            (script_strings.end(), part_of_content.begin(), part_of_content.end());
          script_strings.push_back ('\0');
          script_rec.previous_script = content_part != 0 ? script_id - 1 : 0;
          script_rec.next_script = content_part != (content_parts - 1) ? script_id + 1 : 0;
          ++script_id;

          script_recs.push_back (script_rec);
        }
      }

      ++package_member_id;
      package_member_recs.push_back (package_member_rec);
    }
    package_recs.push_back (package_rec);
  }

  write_file ( output_dir / scene_script_filename
             , put_records (script_recs, script_strings)
             );
  write_file ( output_dir / scene_script_package_filename
             , put_records (package_recs, package_strings)
             );
  write_file ( output_dir / scene_script_package_member_filename
             , put_records (package_member_recs, package_member_strings)
             );

  return 0;
}
