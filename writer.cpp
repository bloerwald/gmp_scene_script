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

template<typename Raw, typename Rec>
std::vector<char> put_records (std::vector<Rec> const& recs)
{
  std::vector<char> data (sizeof (DB2Header) + sizeof (Raw) * recs.size());
  uint32_t filesize (0);

  {
    DB2Header* header (reinterpret_cast<DB2Header*> (data.data()));

    header->magic = '6BDW';
    header->record_count = recs.size();
    header->field_count = Raw::field_count - 1;
    static_assert (sizeof (Rec) % 4 == 0, "assume all-4byte-fields");
    header->record_size = sizeof (Raw);
    header->table_hash = Raw::table_hash;
    header->layout_hash = Raw::layout_hash;

    header->min_id = 0x7fffffff;
    header->max_id = 0;
    for (auto const& rec : recs)
    {
      header->min_id = std::min<uint32_t> (header->min_id, rec.id);
      header->max_id = std::max<uint32_t> (header->max_id, rec.id);
    }

    header->locale = -1;
    header->copy_table_size = 0;
    header->flags = 4;
    header->id_index = 0;
    header->total_field_count = header->field_count;
    header->common_data_table_size = 0;
  }
  filesize += sizeof (DB2Header);

  data.resize (data.size() + sizeof (uint32_t) * (Raw::field_count - 1));
  std::copy ( (char const*)Raw::field_layout.data(), (char const*) (Raw::field_layout.data() + Raw::field_layout.size())
            , data.data() + filesize
            );
  filesize += (sizeof (uint32_t) * (Raw::field_count - 1));

  std::vector<Raw> raws;
  std::vector<uint32_t> ids;
  std::vector<char> stringblock;
  stringblock.emplace_back (0); stringblock.emplace_back (0);
  for (auto const& rec : recs)
  {
    uint32_t id (rec.id);
    raws.emplace_back (rec.raw (stringblock));
    ids.emplace_back (id);
  }
  std::copy ( (char*)raws.data(), (char*) (raws.data() + raws.size())
            , data.data() + filesize
            );
  filesize += raws.size() * sizeof (Raw);

  data.insert (data.end(), stringblock.begin(), stringblock.end());
  {
    DB2Header* header (reinterpret_cast<DB2Header*> (data.data()));
    header->string_table_size = stringblock.size();
  }
  filesize += stringblock.size();

  data.resize (data.size() + ids.size() * 4);
  std::copy ( (char*)ids.data(), (char*) (ids.data() + ids.size())
            , data.data() + filesize
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
    ("DBFilesClient_out/SceneScript.db2");
  const std::string scene_script_package_filename
    ("DBFilesClient_out/SceneScriptPackage.db2");
  const std::string scene_script_package_member_filename
    ("DBFilesClient_out/SceneScriptPackageMember.db2");

  const boost::filesystem::path input_dir (boost::filesystem::current_path() / "scene_scripts");
  const boost::filesystem::path output_dir (boost::filesystem::current_path());//("/Applications/World of Warcraft Public Test");
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

  std::vector<SceneScriptPackageRec> package_recs;
  std::vector<SceneScriptPackageMemberRec> package_member_recs;
  std::vector<SceneScriptRec> script_recs;

  int package_member_id (1);
  int script_id (1);

  for (std::pair<int, package_t> package : packages)
  {
    SceneScriptPackageRec package_rec;
    package_rec.id = package.first;
    package_rec.name = package.second.name;

    for (std::pair<int, package_member_t> member : package.second.members)
    {
      SceneScriptPackageMemberRec package_member_rec;
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

          SceneScriptRec script_rec;
          script_rec.id = script_id;
          script_rec.name = member.second.name;
          script_rec.content = part_of_content;
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
             , put_records<SceneScriptRecRaw> (script_recs)
             );
  write_file ( output_dir / scene_script_package_filename
             , put_records<SceneScriptPackageRecRaw> (package_recs)
             );
  write_file ( output_dir / scene_script_package_member_filename
             , put_records<SceneScriptPackageMemberRecRaw> (package_member_recs)
             );

  return 0;
}
