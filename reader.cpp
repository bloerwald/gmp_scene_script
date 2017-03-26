#include <boost/filesystem.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "structures.hpp"

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

template<typename Rec, typename RawRec>
std::vector<Rec> get_records (std::vector<char> const& data)
{
  const DB2Header* header
    (reinterpret_cast<const DB2Header*> (data.data()));
  if (header->magic != '6BDW') throw std::invalid_argument ("bad header");
  if (header->field_count != header->total_field_count) throw std::invalid_argument ("uses common data");
  if (header->flags != 4) throw std::invalid_argument ("unknown flags");
  if (header->record_size != sizeof (RawRec)) throw std::invalid_argument ("sizeof Raw mismatch");
  if (header->layout_hash != RawRec::layout_hash) throw std::invalid_argument ("layout hash mismatch");

  const RawRec* raw_records
    (reinterpret_cast<const RawRec*> (data.data() + sizeof (DB2Header) + sizeof (uint32_t) * header->field_count));
  const std::vector<RawRec> raw_records_vec (raw_records, raw_records + header->record_count);

  const char* stringblock
    (reinterpret_cast<char const*> (raw_records + header->record_count));

  uint32_t const* ids
    (reinterpret_cast<uint32_t const*> (stringblock + header->string_table_size));

  std::vector<Rec> records;
  std::size_t i (0);
  for (RawRec const& rec : raw_records_vec)
  {
    records.push_back (rec.unraw (*header, stringblock, header->flags & 4 ? ids[i] : -1));
    ++i;
  }

  struct copy_table_entry
  {
    uint32_t id_of_new_row;
    uint32_t id_of_copied_row;
  };
  copy_table_entry const* copies
    (reinterpret_cast<copy_table_entry const*> (header->flags & 4 ? ids + header->record_count : ids));

  for (std::size_t i (0); i < header->copy_table_size / sizeof (copy_table_entry); ++i)
  {
    auto it (std::find_if (records.begin(), records.end(), [&] (Rec const& rec)
                 {
                   return rec.id == copies[i].id_of_copied_row;
                 }
                          ));
    records.push_back (it->clone (copies[i].id_of_new_row));
  }

  return records;
}

std::string replace_not_permitted_characters (std::string str)
{
  std::replace (str.begin(), str.end(), '/', ',');
  return str;
}

int main (int argc, char** argv)
{
  const std::string scene_script_filename
    ("DBFilesClient/SceneScript.db2");
  const std::string scene_script_package_filename
    ("DBFilesClient/SceneScriptPackage.db2");
  const std::string scene_script_package_member_filename
    ("DBFilesClient/SceneScriptPackageMember.db2");

  const boost::filesystem::path output_dir (boost::filesystem::current_path() / "scene_scripts");
  const boost::filesystem::path by_name_dir (output_dir / "by name");
  const boost::filesystem::path by_id_dir (output_dir / "by id");

  boost::filesystem::create_directories (output_dir);
  boost::filesystem::create_directories (by_name_dir);

  const std::vector<char> scene_script
    (read_file (scene_script_filename));
  const std::vector<char> scene_script_package
    (read_file (scene_script_package_filename));
  const std::vector<char> scene_script_package_member
    (read_file (scene_script_package_member_filename));

  const std::vector<SceneScriptRec> scene_script_records
    (get_records<SceneScriptRec, SceneScriptRecRaw> (scene_script));
  const std::vector<SceneScriptPackageRec> scene_script_package_records
    (get_records<SceneScriptPackageRec, SceneScriptPackageRecRaw> (scene_script_package));
  const std::vector<SceneScriptPackageMemberRec> scene_script_package_member_records
    (get_records<SceneScriptPackageMemberRec, SceneScriptPackageMemberRecRaw> (scene_script_package_member));

  std::map<int, std::vector<SceneScriptRec>::const_iterator> scene_script_records_by_id;

  for ( std::vector<SceneScriptRec>::const_iterator it (scene_script_records.cbegin())
      ; it != scene_script_records.cend()
      ; ++it
      )
  {
    scene_script_records_by_id.emplace (it->id, it);
  }

  for (SceneScriptPackageRec const& rec : scene_script_package_records)
  {
    boost::filesystem::path dir (by_id_dir / std::to_string (rec.id));
    boost::filesystem::create_directories (dir);
    std::ofstream name_file ((dir / "name.txt").string());
    name_file << rec.name;
    std::ofstream id_file ((dir / "id.txt").string());
    id_file << rec.id;
    boost::filesystem::path link_dir (by_name_dir / replace_not_permitted_characters (rec.name));
    boost::filesystem::create_symlink (dir, link_dir);
  }

  for (SceneScriptPackageMemberRec const& rec : scene_script_package_member_records)
  {
    boost::filesystem::path dir (by_id_dir / std::to_string (rec.package));
    if (rec.script != 0)
    {
      std::string name;
      for (int next_script = rec.script; next_script != 0;)
      {
        const std::vector<SceneScriptRec>::const_iterator script_rec
          (scene_script_records_by_id.at (next_script));
        if (name.empty())
        {
          name = script_rec->name;
        }
        std::ofstream name_file ((dir / (std::to_string
          (rec.sequence) + "." + name + ".lua")).string(), std::ios_base::app);
        name_file << script_rec->content;
        next_script = script_rec->next_script;
      }
    }
    else if (rec.d != 0)
    {
      boost::filesystem::path included (by_id_dir / std::to_string (rec.d));
      std::ifstream inc_name_stream ((included / "name.txt").string());
      std::string inc_name;
      std::getline (inc_name_stream, inc_name);
      boost::filesystem::path link_dir (dir / (std::to_string (rec.sequence) + "." + inc_name + ".inc"));
      boost::filesystem::create_directory_symlink (included, link_dir);
    }
    else
    {
      std::cerr << rec.id << ": neither script nor include\n";
    }
  }

  return 0;
}
