
/*

  KLayout Layout Viewer
  Copyright (C) 2006-2018 Matthias Koefferlein

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


#include "dbDeepShapeStore.h"
#include "tlUnitTest.h"
#include "tlStream.h"

TEST(1)
{
  db::DeepShapeStore store;
  db::Layout layout;

  unsigned int l1 = layout.insert_layer ();
  unsigned int l2 = layout.insert_layer ();
  db::cell_index_type c1 = layout.add_cell ("C1");
  db::cell_index_type c2 = layout.add_cell ("C2");

  EXPECT_EQ (store.layouts (), (unsigned int) 0);

  db::DeepLayer dl1 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l1));
  db::DeepLayer dl2 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l2));

  EXPECT_EQ (dl1.layer (), l1);
  EXPECT_EQ (dl2.layer (), l2);
  EXPECT_EQ (&dl1.layout (), &dl2.layout ());
  EXPECT_EQ (store.layouts (), (unsigned int) 1);

  db::DeepLayer dl3 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c2), l1));
  EXPECT_EQ (dl3.layer (), l1);
  EXPECT_NE (&dl1.layout (), &dl3.layout ());
  EXPECT_EQ (store.layouts (), (unsigned int) 2);

  db::DeepLayer dl4 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l1, db::Box (0, 1, 2, 3)));
  db::DeepLayer dl5 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l2, db::Box (0, 1, 2, 3)));
  EXPECT_EQ (dl4.layer (), l1);
  EXPECT_EQ (dl5.layer (), l1);  //  not l2, because it's a new layout
  EXPECT_EQ (store.layouts (), (unsigned int) 4);

  db::DeepLayer dl6 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l1, db::Box (0, 1, 2, 3)));
  EXPECT_EQ (dl6.layer (), l2);  //  a new layer (a copy)
  EXPECT_EQ (&dl6.layout (), &dl4.layout ());
  EXPECT_EQ (store.layouts (), (unsigned int) 4);
}

static size_t shapes_in_top (const db::Layout &layout, unsigned int layer)
{
  const db::Cell &top = layout.cell (*layout.begin_top_down ());
  return top.shapes (layer).size ();
}

TEST(2_RefCounting)
{
  db::DeepShapeStore store;
  db::Layout layout;

  unsigned int l1 = layout.insert_layer ();
  unsigned int l2 = layout.insert_layer ();
  db::cell_index_type c1 = layout.add_cell ("C1");
  db::cell_index_type c2 = layout.add_cell ("C2");
  layout.cell (c1).shapes (l1).insert (db::Box (0, 1, 2, 3));
  layout.cell (c1).shapes (l2).insert (db::Box (0, 1, 2, 3));

  EXPECT_EQ (store.layouts (), (unsigned int) 0);

  db::DeepLayer dl1 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l1));
  db::DeepLayer dl2 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l2));
  db::DeepLayer dl3 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c2), l1));
  db::DeepLayer dl4 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l1, db::Box (0, 1, 2, 3)));
  db::DeepLayer dl5 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l2, db::Box (0, 1, 2, 3)));
  db::DeepLayer dl6 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l1, db::Box (0, 1, 2, 3)));

  EXPECT_EQ (store.layouts (), (unsigned int) 4);

  unsigned int lyi1 = dl1.layout_index ();
  unsigned int lyi2 = dl2.layout_index ();
  unsigned int lyi3 = dl3.layout_index ();
  unsigned int lyi4 = dl4.layout_index ();
  unsigned int lyi5 = dl5.layout_index ();
  unsigned int lyi6 = dl6.layout_index ();

  EXPECT_EQ (lyi1, lyi2);
  EXPECT_NE (lyi3, lyi2);
  EXPECT_NE (lyi5, lyi4);
  EXPECT_NE (lyi5, lyi3);
  EXPECT_EQ (lyi6, lyi4);

  EXPECT_EQ (dl1.layer (), l1);
  EXPECT_EQ (dl2.layer (), l2);
  EXPECT_EQ (dl4.layer (), l1);
  EXPECT_EQ (dl6.layer (), l2);

  //  dl1 and dl2 share the same layout, but not the same layer
  //  dl4 and dl6 share the same layout, but not the same layer

  EXPECT_EQ (store.is_valid_layout_index (lyi6), true);
  EXPECT_EQ (store.is_valid_layout_index (lyi5), true);
  EXPECT_EQ (store.is_valid_layout_index (lyi3), true);
  EXPECT_EQ (store.is_valid_layout_index (lyi1), true);

  EXPECT_EQ (shapes_in_top (store.const_layout (lyi6), l2), size_t (1));
  dl6 = db::DeepLayer ();
  EXPECT_EQ (shapes_in_top (store.const_layout (lyi6), l2), size_t (0));

  EXPECT_EQ (shapes_in_top (store.const_layout (lyi6), l1), size_t (1));
  db::DeepLayer dl4a = dl4;
  dl4 = db::DeepLayer ();
  EXPECT_EQ (shapes_in_top (store.const_layout (lyi6), l1), size_t (1));
  dl4a = db::DeepLayer ();
  EXPECT_EQ (store.is_valid_layout_index (lyi6), false);

  dl3 = db::DeepLayer ();
  EXPECT_EQ (store.is_valid_layout_index (lyi3), false);

  {
    db::DeepLayer dl5a = dl5;
    db::DeepLayer dl5b = dl5a;
    dl5 = db::DeepLayer ();
    EXPECT_EQ (store.is_valid_layout_index (lyi5), true);
  }
  EXPECT_EQ (store.is_valid_layout_index (lyi5), false);

  EXPECT_EQ (shapes_in_top (store.const_layout (lyi1), l1), size_t (1));
  EXPECT_EQ (shapes_in_top (store.const_layout (lyi1), l2), size_t (1));

  dl1 = db::DeepLayer ();
  EXPECT_EQ (shapes_in_top (store.const_layout (lyi1), l1), size_t (0));
  EXPECT_EQ (shapes_in_top (store.const_layout (lyi1), l2), size_t (1));

  dl2 = db::DeepLayer ();
  EXPECT_EQ (store.is_valid_layout_index (lyi1), false);
}

TEST(3_TextTreatment)
{
  db::DeepShapeStore store;
  db::Layout layout;

  unsigned int l1 = layout.insert_layer ();
  db::cell_index_type c1 = layout.add_cell ("C1");
  layout.cell (c1).shapes (l1).insert (db::Text ("TEXT", db::Trans (db::Vector (1000, 2000))));

  db::DeepLayer dl1 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l1));
  EXPECT_EQ (store.layouts (), (unsigned int) 1);

  EXPECT_EQ (dl1.initial_cell ().shapes (dl1.layer ()).empty (), true);

  store.set_text_enlargement (1);
  dl1 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l1));
  EXPECT_EQ (store.layouts (), (unsigned int) 1);

  EXPECT_EQ (dl1.initial_cell ().shapes (dl1.layer ()).size (), size_t (1));
  EXPECT_EQ (dl1.initial_cell ().shapes (dl1.layer ()).begin (db::ShapeIterator::All)->to_string (), "polygon (999,1999;999,2001;1001,2001;1001,1999)");

  store.set_text_property_name (tl::Variant ("text"));
  dl1 = store.create_polygon_layer (db::RecursiveShapeIterator (layout, layout.cell (c1), l1));
  EXPECT_EQ (store.layouts (), (unsigned int) 1);

  EXPECT_EQ (dl1.initial_cell ().shapes (dl1.layer ()).size (), size_t (1));
  EXPECT_EQ (dl1.initial_cell ().shapes (dl1.layer ()).begin (db::ShapeIterator::All)->to_string (), "polygon (999,1999;999,2001;1001,2001;1001,1999) prop_id=1");

  const db::Layout *dss_layout = &store.const_layout (0);
  db::PropertiesRepository::properties_set ps = dss_layout->properties_repository ().properties (1);
  EXPECT_EQ (ps.size (), size_t (1));
  EXPECT_EQ (dss_layout->properties_repository ().prop_name (ps.begin ()->first).to_string (), "text");
  EXPECT_EQ (ps.begin ()->second.to_string (), "TEXT");
}
