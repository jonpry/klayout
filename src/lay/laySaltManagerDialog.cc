
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

#include "laySaltManagerDialog.h"
#include "laySaltGrainPropertiesDialog.h"
#include "laySalt.h"
#include "ui_SaltGrainTemplateSelectionDialog.h"
#include "tlString.h"

#include <QAbstractItemModel>
#include <QAbstractTextDocumentLayout>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QPainter>
#include <QDir>
#include <QTextStream>
#include <QBuffer>
#include <QResource>

namespace lay
{

// --------------------------------------------------------------------------------------

/**
 *  @brief A model representing the salt grains for a QListView
 */
class SaltModel
  : public QAbstractItemModel
{
public:
  SaltModel (QObject *parent, lay::Salt *salt)
    : QAbstractItemModel (parent), mp_salt (salt)
  {
    //  .. nothing yet ..
  }

  QVariant data (const QModelIndex &index, int role) const
  {
    if (role == Qt::DisplayRole) {

      const lay::SaltGrain *g = mp_salt->begin_flat ()[index.row ()];

      std::string text = "<html><body>";
      text += "<h4>";
      text += tl::escaped_to_html (g->name ());
      if (!g->version ().empty ()) {
        text += " ";
        text += tl::escaped_to_html (g->version ());
      }
      if (!g->title ().empty ()) {
        text += " - ";
        text += tl::escaped_to_html (g->title ());
      }
      text += "</h4>";
      if (!g->doc ().empty ()) {
        text += "<p>";
        text += tl::escaped_to_html (g->doc ());
        text += "</p>";
      }
      text += "</body></html>";

      return tl::to_qstring (text);

    } else if (role == Qt::DecorationRole) {

      int icon_dim = 64;

      const lay::SaltGrain *g = mp_salt->begin_flat ()[index.row ()];
      if (g->icon ().isNull ()) {
        return QIcon (":/salt_icon.png");
      } else {

        QImage img = g->icon ();
        if (img.width () == icon_dim && img.height () == icon_dim) {
          return QPixmap::fromImage (img);
        } else {

          img = img.scaled (QSize (icon_dim, icon_dim), Qt::KeepAspectRatio, Qt::SmoothTransformation);

          QImage final_img (icon_dim, icon_dim, QImage::Format_ARGB32);
          final_img.fill (QColor (0, 0, 0, 0));
          QPainter painter (&final_img);
          painter.drawImage ((icon_dim - img.width ()) / 2, (icon_dim - img.height ()) / 2, img);

          return QPixmap::fromImage (final_img);

        }

      }

    } else {
      return QVariant ();
    }
  }

  QModelIndex index (int row, int column, const QModelIndex &parent) const
  {
    if (parent.isValid ()) {
      return QModelIndex ();
    } else {
      return createIndex (row, column, mp_salt->begin_flat () [row]);
    }
  }

  QModelIndex parent (const QModelIndex & /*index*/) const
  {
    return QModelIndex ();
  }

  int columnCount(const QModelIndex & /*parent*/) const
  {
    return 1;
  }

  int rowCount (const QModelIndex &parent) const
  {
    if (parent.isValid ()) {
      return 0;
    } else {
      return mp_salt->end_flat () - mp_salt->begin_flat ();
    }
  }

  SaltGrain *grain_from_index (const QModelIndex &index) const
  {
    if (index.isValid ()) {
      return static_cast<SaltGrain *> (index.internalPointer ());
    } else {
      return 0;
    }
  }

  void update ()
  {
    //  @@@
  }

public:
  lay::Salt *mp_salt;
};

// --------------------------------------------------------------------------------------

/**
 *  @brief A delegate displaying the summary of a grain
 */
class SaltItemDelegate
  : public QStyledItemDelegate
{
public:
  SaltItemDelegate (QObject *parent)
    : QStyledItemDelegate (parent)
  {
    // .. nothing yet ..
  }

  void paint (QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
  {
    QStyleOptionViewItemV4 optionV4 = option;
    initStyleOption (&optionV4, index);

    QStyle *style = optionV4.widget ? optionV4.widget->style () : QApplication::style ();

    QTextDocument doc;
    doc.setHtml (optionV4.text);

    optionV4.text = QString ();
    style->drawControl (QStyle::CE_ItemViewItem, &optionV4, painter);

    QAbstractTextDocumentLayout::PaintContext ctx;

    if (optionV4.state & QStyle::State_Selected) {
      ctx.palette.setColor (QPalette::Text, optionV4.palette.color (QPalette::Active, QPalette::HighlightedText));
    }

    QRect textRect = style->subElementRect (QStyle::SE_ItemViewItemText, &optionV4);
    painter->save ();
    painter->translate (textRect.topLeft ());
    painter->setClipRect (textRect.translated (-textRect.topLeft ()));
    doc.documentLayout()->draw (painter, ctx);
    painter->restore ();
  }

  QSize sizeHint (const QStyleOptionViewItem &option, const QModelIndex &index) const
  {
    const int textWidth = 500;

    QStyleOptionViewItemV4 optionV4 = option;
    initStyleOption (&optionV4, index);

    const QListView *view = dynamic_cast<const QListView *> (optionV4.widget);
    QSize icon_size (0, 0);
    if (view) {
      icon_size = view->iconSize ();
    }

    QTextDocument doc;
    doc.setHtml (optionV4.text);
    doc.setTextWidth (textWidth);
    return QSize (textWidth + icon_size.width () + 6, std::max (icon_size.height () + 12, int (doc.size ().height ())));
  }
};

// --------------------------------------------------------------------------------------

/**
 *  @brief A tiny dialog to select a template and a name for the grain
 */
class SaltGrainTemplateSelectionDialog
  : public QDialog, private Ui::SaltGrainTemplateSelectionDialog
{
public:
  SaltGrainTemplateSelectionDialog (QWidget *parent)
    : QDialog (parent)
  {
    Ui::SaltGrainTemplateSelectionDialog::setupUi (this);

    m_salt_templates.add_location (":/salt_templates");
    salt_view->setModel (new SaltModel (this, &m_salt_templates));
    salt_view->setItemDelegate (new SaltItemDelegate (this));
    salt_view->setCurrentIndex (salt_view->model ()->index (0, 0, QModelIndex ()));
  }

  lay::SaltGrain templ () const
  {
    SaltModel *model = dynamic_cast<SaltModel *> (salt_view->model ());
    tl_assert (model != 0);

    SaltGrain *g = model->grain_from_index (salt_view->currentIndex ());
    tl_assert (g != 0);

    g->set_name (tl::to_string (name_edit->text ().simplified ()));

    return *g;
  }

  void accept ()
  {
    name_alert->clear ();
    std::string name = tl::to_string (name_edit->text ().simplified ());
    if (name.empty ()) {
      name_alert->error () << tr ("Name must not be empty");
    } else if (! SaltGrain::valid_name (name)) {
      name_alert->error () << tr ("Name is not valid (must be composed of letters, digits or underscores.\nGroups and names need to be separated with slashes.");
    } else {
      QDialog::accept ();
    }
  }

private:
  lay::Salt m_salt_templates;
};

// --------------------------------------------------------------------------------------
//  SaltManager implementation

// @@@
lay::Salt salt;
static bool salt_initialized = false;
void make_salt ()
{
  if (!salt_initialized) {
    salt_initialized = true;
    salt.add_location (tl::to_string (QDir::homePath () + QString::fromUtf8("/.klayout/salt")));
  }
}
// @@@

SaltManagerDialog::SaltManagerDialog (QWidget *parent)
  : QDialog (parent),
    m_current_changed_enabled (true)
{
  Ui::SaltManagerDialog::setupUi (this);
  mp_properties_dialog = new lay::SaltGrainPropertiesDialog (this);

  connect (edit_button, SIGNAL (clicked ()), this, SLOT (edit_properties ()));
  connect (create_button, SIGNAL (clicked ()), this, SLOT (create_grain ()));
  connect (delete_button, SIGNAL (clicked ()), this, SLOT (delete_grain ()));
  connect (install_button, SIGNAL (clicked ()), this, SLOT (install_grain ()));

// @@@
  salt = lay::Salt (); salt_initialized = false;
  make_salt ();
  mp_salt = &salt;
// @@@

  SaltModel *model = new SaltModel (this, mp_salt);
  salt_view->setModel (model);
  salt_view->setItemDelegate (new SaltItemDelegate (this));

  connect (mp_salt, SIGNAL (collections_changed ()), this, SLOT (salt_changed ()));

  //  select the first grain
  if (model->rowCount (QModelIndex ()) > 0) {
    salt_view->setCurrentIndex (model->index (0, 0, QModelIndex ()));
  }

  salt_changed ();

  connect (salt_view->selectionModel (), SIGNAL (currentChanged (const QModelIndex &, const QModelIndex &)), this, SLOT (current_changed ()));

  // @@@
}

void
SaltManagerDialog::edit_properties ()
{
  SaltGrain *g = current_grain ();
  if (g) {
    if (mp_properties_dialog->exec_dialog (g, mp_salt)) {
      current_changed ();
      // @@@
    }
  }
}

// @@@
namespace
{

/**
 *  @brief A helper class required because directory traversal is not supported by QResource directly
 */
class OpenResource
  : public QResource
{
public:
  using QResource::isDir;
  using QResource::isFile;
  using QResource::children;

  OpenResource (const QString &path)
    : QResource (path)
  {
    //  .. nothing yet ..
  }
};

}
// @@@

void
SaltManagerDialog::create_grain ()
{
  SaltGrainTemplateSelectionDialog temp_dialog (this);
  if (temp_dialog.exec ()) {

    // @@@

  }
}

void
SaltManagerDialog::delete_grain ()
{

  // @@@

}

void
SaltManagerDialog::install_grain ()
{

  // @@@

}

void
SaltManagerDialog::salt_changed ()
{
  SaltModel *model = dynamic_cast <SaltModel *> (salt_view->model ());
  if (! model) {
    return;
  }

  m_current_changed_enabled = false;
  model->update ();
  m_current_changed_enabled = true;

  if (mp_salt->is_empty ()) {
    list_stack->setCurrentIndex (1);
    details_frame->hide ();
  } else {
    list_stack->setCurrentIndex (0);
    details_frame->show ();
  }

  current_changed ();
}

void
SaltManagerDialog::current_changed ()
{
  SaltGrain *g = current_grain ();
  details_text->set_grain (g);
  if (!g) {
    details_frame->setEnabled (false);
    delete_button->setEnabled (false);
  } else {
    details_frame->setEnabled (true);
    delete_button->setEnabled (true);
    edit_button->setEnabled (! g->is_readonly ());
  }
}

lay::SaltGrain *
SaltManagerDialog::current_grain ()
{
  SaltModel *model = dynamic_cast <SaltModel *> (salt_view->model ());
  return model ? model->grain_from_index (salt_view->currentIndex ()) : 0;
}

}
