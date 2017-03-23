
/*

  KLayout Layout Viewer
  Copyright (C) 2006-2017 Matthias Koefferlein

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "laySalt.h"
#include "laySaltDownloadManager.h"
#include "tlString.h"
#include "tlFileUtils.h"
#include "tlLog.h"
#include "tlInternational.h"

#include <QFileInfo>
#include <QDir>

namespace lay
{

Salt::Salt ()
{
  //  .. nothing yet ..
}

Salt::Salt (const Salt &other)
  : QObject ()
{
  operator= (other);
}

Salt &Salt::operator= (const Salt &other)
{
  if (this != &other) {
    m_root = other.m_root;
  }
  return *this;
}

Salt::flat_iterator
Salt::begin_flat ()
{
  validate ();
  return mp_flat_grains.begin ();
}

Salt::flat_iterator
Salt::end_flat ()
{
  validate ();
  return mp_flat_grains.end ();
}

SaltGrain *
Salt::grain_by_name (const std::string &name)
{
  validate ();
  std::map<std::string, SaltGrain *>::const_iterator g = m_grains_by_name.find (name);
  if (g != m_grains_by_name.end ()) {
    return g->second;
  } else {
    return 0;
  }
}

void
Salt::add_location (const std::string &path)
{
  tl_assert (! path.empty ());

  if (path[0] != ':') {
    //  do nothing if the collection is already there
    QFileInfo fi (tl::to_qstring (path));
    for (lay::SaltGrains::collection_iterator g = m_root.begin_collections (); g != m_root.end_collections (); ++g) {
      if (QFileInfo (tl::to_qstring (g->path ())) == fi) {
        return;
      }
    }
  }

  lay::SaltGrains gg = lay::SaltGrains::from_path (path);
  m_root.add_collection (gg);
  invalidate ();
}

void
Salt::remove_location (const std::string &path)
{
  QFileInfo fi (tl::to_qstring (path));
  for (lay::SaltGrains::collection_iterator g = m_root.begin_collections (); g != m_root.end_collections (); ++g) {
    if (QFileInfo (tl::to_qstring (g->path ())) == fi) {
      m_root.remove_collection (g, false);
      invalidate ();
      return;
    }
  }
}

void
Salt::refresh ()
{
  lay::SaltGrains new_root;
  for (lay::SaltGrains::collection_iterator g = m_root.begin_collections (); g != m_root.end_collections (); ++g) {
    new_root.add_collection (lay::SaltGrains::from_path (g->path ()));
  }
  if (new_root != m_root) {
    m_root = new_root;
    invalidate ();
  }
}

void
Salt::add_collection_to_flat (SaltGrains &gg)
{
  for (lay::SaltGrains::grain_iterator g = gg.begin_grains (); g != gg.end_grains (); ++g) {
    //  TODO: get rid of the const cast - would require a non-const grain iterator
    mp_flat_grains.push_back (const_cast <SaltGrain *> (g.operator-> ()));
  }
  for (lay::SaltGrains::collection_iterator g = gg.begin_collections (); g != gg.end_collections (); ++g) {
    //  TODO: get rid of the const cast - would require a non-const grain collection iterator
    add_collection_to_flat (const_cast <SaltGrains &> (*g));
  }
}

namespace {

struct NameCompare
{
  bool operator () (lay::SaltGrain *a, lay::SaltGrain *b) const
  {
    //  TODO: UTF-8 support?
    return a->name () < b->name ();
  }
};

}

void
Salt::validate ()
{
  if (mp_flat_grains.empty ()) {

    add_collection_to_flat (m_root);

    m_grains_by_name.clear ();
    for (std::vector<SaltGrain *>::const_iterator i = mp_flat_grains.begin (); i != mp_flat_grains.end (); ++i) {
      m_grains_by_name.insert (std::make_pair ((*i)->name (), *i));
    }

    //  NOTE: we intentionally sort after the name list has been built - this way
    //  the first entry will win in the name to grain map.
    std::sort (mp_flat_grains.begin (), mp_flat_grains.end (), NameCompare ());

  }
}

void
Salt::invalidate ()
{
  mp_flat_grains.clear ();
  emit collections_changed ();
}


static
bool remove_from_collection (SaltGrains &collection, const std::string &name)
{
  bool res = false;

  for (SaltGrains::grain_iterator g = collection.begin_grains (); g != collection.end_grains (); ++g) {
    if (g->name () == name) {
      SaltGrains::grain_iterator gnext = g;
      ++gnext;
      collection.remove_grain (g, true);
      res = true;
    }
  }

  for (SaltGrains::collection_iterator gg = collection.begin_collections (); gg != collection.end_collections (); ++gg) {
    //  TODO: remove this const_cast
    if (remove_from_collection (const_cast <SaltGrains &> (*gg), name)) {
      res = true;
    }
  }

  return res;
}

bool
Salt::remove_grain (const SaltGrain &grain)
{
  tl::info << QObject::tr ("Removing package '%1' ..").arg (tl::to_qstring (grain.name ()));
  if (remove_from_collection (m_root, grain.name ())) {
    tl::info << QObject::tr ("Package '%1' removed.").arg (tl::to_qstring (grain.name ()));
    invalidate ();
    return true;
  } else {
    tl::warn << QObject::tr ("Failed to remove package '%1'.").arg (tl::to_qstring (grain.name ()));
    return false;
  }
}

bool
Salt::create_grain (const SaltGrain &templ, SaltGrain &target, SaltDownloadManager &download_manager)
{
  tl_assert (!m_root.is_empty ());

  const SaltGrains *coll = m_root.begin_collections ().operator-> ();

  std::string path = target.path ();
  if (! path.empty ()) {
    coll = 0;
    for (SaltGrains::collection_iterator gg = m_root.begin_collections (); gg != m_root.end_collections (); ++gg) {
      if (tl::is_parent_path (tl::to_qstring (gg->path ()), tl::to_qstring (path))) {
        coll = gg.operator-> ();
        break;
      }
    }
    tl_assert (coll != 0);
  }

  tl::info << QObject::tr ("Installing package '%1' ..").arg (tl::to_qstring (target.name ()));

  QDir target_dir (tl::to_qstring (coll->path ()));

  try {

    //  change down to the desired target location and create the directory structure while doing so
    std::vector<std::string> name_parts = tl::split (target.name (), "/");
    for (std::vector<std::string>::const_iterator n = name_parts.begin (); n != name_parts.end (); ++n) {
      QDir subdir (target_dir.filePath (tl::to_qstring (*n)));
      if (! subdir.exists ()) {
        if (! target_dir.mkdir (tl::to_qstring (*n))) {
          throw tl::Exception (tl::to_string (tr ("Unable to create target directory '%1' for installing package").arg (subdir.path ())));
        }
        if (! target_dir.cd (tl::to_qstring (*n))) {
          throw tl::Exception (tl::to_string (tr ("Unable to change to target directory '%1' for installing package").arg (subdir.path ())));
        }
      }
    }

  } catch (tl::Exception &ex) {
    tl::error << ex.msg ();
    return false;
  }

  bool res = true;

  target = templ;
  target.set_path (tl::to_string (target_dir.absolutePath ()));

  if (! templ.path ().empty ()) {

    //  if the template represents an actual folder, use the files from there
    tl::info << QObject::tr ("Copying package from '%1' to '%2' ..").arg (tl::to_qstring (templ.path ())).arg (tl::to_qstring (target.path ()));
    res = tl::cp_dir_recursive (templ.path (), target.path ());

  } else if (! templ.url ().empty ()) {

    //  otherwise download from the URL
    tl::info << QObject::tr ("Downloading package from '%1' to '%2' ..").arg (tl::to_qstring (templ.url ())).arg (tl::to_qstring (target.path ()));
    res = download_manager.download (templ.url (), target.path ());

  }

  if (res) {

    tl::info << QObject::tr ("Package '%1' installed").arg (tl::to_qstring (target.name ()));

    target.set_installed_time (QDateTime::currentDateTime ());
    target.save ();

  } else {

    tl::warn << QObject::tr ("Failed to install package '%1' - removing files ..").arg (tl::to_qstring (target.name ()));
    if (! tl::rm_dir_recursive (target.path ())) {
      tl::warn << QObject::tr ("Failed to remove files").arg (tl::to_qstring (target.name ()));
    }

  }

  return res;
}

}
