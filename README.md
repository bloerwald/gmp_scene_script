reader produces a tree of 

    scene_scripts/
      by_name/
        [names…] -> symlink to id
      by_id/
        [ids…]/
          id.txt -> id again
          name.txt -> name of script
          ${order}.${name}.(inc|lua) -> thread of script or include to other i

which can be edited and converted back to db2 wiht writer.
