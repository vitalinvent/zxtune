/*
Abstract:
  Playlist table view implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "table_view.h"
#include "playlist/supp/controller.h"
#include "playlist/supp/model.h"
#include "ui/utils.h"
//common includes
#include <contract.h>
#include <logging.h>
//boost includes
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
//qt includes
#include <QtCore/QUrl>
#include <QtGui/QHeaderView>

namespace
{
  const std::string THIS_MODULE("Playlist::UI::TableView");

  //Options
  const char FONT_FAMILY[] = "Arial";
  const int_t FONT_SIZE = 8;
  const int_t ROW_HEIGTH = 16;
  const int_t ICON_WIDTH = 24;
  const int_t DISPLAYNAME_WIDTH = 320;
  const int_t DURATION_WIDTH = 60;

  class TableViewImpl : public Playlist::UI::TableView
  {
  public:
    TableViewImpl(QWidget& parent, const Playlist::Item::StateCallback& callback,
      Playlist::Model::Ptr model)
      : Playlist::UI::TableView(parent)
      , Font(QString::fromUtf8(FONT_FAMILY), FONT_SIZE)
    {
      //setup self
      setSortingEnabled(true);
      setModel(model);
      setItemDelegate(Playlist::UI::TableViewItem::Create(*this, callback));
      setFont(Font);
      setMinimumSize(256, 128);
      //setup ui
      setAcceptDrops(true);
      setDragEnabled(true);
      setDragDropMode(QAbstractItemView::InternalMove);
      setDropIndicatorShown(true);
      setEditTriggers(QAbstractItemView::NoEditTriggers);
      setSelectionMode(QAbstractItemView::ExtendedSelection);
      setSelectionBehavior(QAbstractItemView::SelectRows);
      setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
      setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
      setShowGrid(false);
      setGridStyle(Qt::NoPen);
      setWordWrap(false);
      setCornerButtonEnabled(false);
      //setup dynamic ui
      if (QHeaderView* const horHeader = horizontalHeader())
      {
        horHeader->setObjectName("Columns");
        horHeader->setFont(Font);
        horHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        horHeader->setHighlightSections(false);
        horHeader->setTextElideMode(Qt::ElideRight);
        horHeader->setMovable(true);
        horHeader->resizeSection(Playlist::Model::COLUMN_TYPE, ICON_WIDTH);
        horHeader->resizeSection(Playlist::Model::COLUMN_DISPLAY_NAME, DISPLAYNAME_WIDTH);
        horHeader->resizeSection(Playlist::Model::COLUMN_DURATION, DURATION_WIDTH);
      }
      if (QHeaderView* const verHeader = verticalHeader())
      {
        verHeader->setFont(Font);
        verHeader->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
        verHeader->setDefaultSectionSize(ROW_HEIGTH);
        verHeader->setResizeMode(QHeaderView::Fixed);
      }

      //signals
      Require(connect(this, SIGNAL(activated(const QModelIndex&)), SLOT(ActivateItem(const QModelIndex&))));

      Log::Debug(THIS_MODULE, "Created at %1%", this);
    }

    virtual ~TableViewImpl()
    {
      Log::Debug(THIS_MODULE, "Destroyed at %1%", this);
    }

    virtual Playlist::Model::IndexSetPtr GetSelectedItems() const
    {
      const QItemSelectionModel* const selection = selectionModel();
      const QModelIndexList& items = selection->selectedRows();
      const boost::shared_ptr<Playlist::Model::IndexSet> result = boost::make_shared<Playlist::Model::IndexSet>();
      std::for_each(items.begin(), items.end(),
                    boost::bind(boost::mem_fn<std::pair<Playlist::Model::IndexSet::iterator, bool>, Playlist::Model::IndexSet, const Playlist::Model::IndexSet::value_type&>(&Playlist::Model::IndexSet::insert), result.get(),
          boost::bind(&QModelIndex::row, _1)));
      return result;
    }

    virtual void SelectItems(const Playlist::Model::IndexSet& indices)
    {
      setEnabled(true);
      QAbstractItemModel* const curModel = model();
      QItemSelectionModel* const selectModel = selectionModel();
      QItemSelection selection;
      for (Playlist::Model::IndexSet::const_iterator it = indices.begin(), lim = indices.end(); it != lim; ++it)
      {
        const QModelIndex left = curModel->index(*it, 0);
        const QModelIndex right = curModel->index(*it, Playlist::Model::COLUMNS_COUNT - 1);
        const QItemSelection sel(left, right);
        selection.merge(sel, QItemSelectionModel::Select);
      }
      selectModel->select(selection, QItemSelectionModel::ClearAndSelect);
      if (!indices.empty())
      {
        ActivateTableRow(*indices.rbegin());
      }
    }

    virtual void SelectItems(Playlist::Model::IndexSetPtr indices)
    {
      return SelectItems(*indices);
    }

    virtual void ActivateTableRow(unsigned index)
    {
      QAbstractItemModel* const curModel = model();
      const QModelIndex idx = curModel->index(index, 0);
      scrollTo(idx, QAbstractItemView::EnsureVisible);
    }

    virtual void ActivateItem(const QModelIndex& index)
    {
      if (index.isValid())
      {
        const unsigned number = index.row();
        emit TableRowActivated(number);
      }
    }
  private:
    QFont Font;
  };

  class TableViewItemImpl : public Playlist::UI::TableViewItem
  {
  public:
    TableViewItemImpl(QWidget& parent,
      const Playlist::Item::StateCallback& callback)
      : Playlist::UI::TableViewItem(parent)
      , Callback(callback)
      , Palette()
    {
    }

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
      QStyleOptionViewItem fixedOption(option);
      FillItemStyle(index, fixedOption);
      Playlist::UI::TableViewItem::paint(painter, fixedOption, index);
    }
  private:
    void FillItemStyle(const QModelIndex& index, QStyleOptionViewItem& style) const
    {
      //disable focus to remove per-cell dotted border
      style.state &= ~QStyle::State_HasFocus;
      const Playlist::Item::State state = Callback.GetState(index);
      if (state == Playlist::Item::STOPPED)
      {
        return;
      }
      const QBrush& textColor = Palette.text();
      const QBrush& baseColor = Palette.base();
      const QBrush& highlightColor = Palette.highlight();
      const QBrush& disabledColor = Palette.mid();

      QPalette& palette = style.palette;
      const bool isSelected = 0 != (style.state & QStyle::State_Selected);
      //force selection to apply only 2 brushes instead of 3
      style.state |= QStyle::State_Selected;

      switch (state)
      {
      case Playlist::Item::PLAYING:
        {
          //invert, propagate selection to text
          const QBrush& itemText = isSelected ? highlightColor : baseColor;
          const QBrush& itemBack = textColor;
          palette.setBrush(QPalette::HighlightedText, itemText);
          palette.setBrush(QPalette::Highlight, itemBack);
        }
        break;
      case Playlist::Item::PAUSED:
        {
          //disable bg, propagate selection to text
          const QBrush& itemText = isSelected ? highlightColor : baseColor;
          const QBrush& itemBack = disabledColor;
          palette.setBrush(QPalette::HighlightedText, itemText);
          palette.setBrush(QPalette::Highlight, itemBack);
        }
        break;
      case Playlist::Item::ERROR:
        {
          //disable text
          const QBrush& itemText = disabledColor;
          const QBrush& itemBack = isSelected ? highlightColor : baseColor;
          palette.setBrush(QPalette::HighlightedText, itemText);
          palette.setBrush(QPalette::Highlight, itemBack);
        }
        break;
      default:
        assert(!"Invalid playitem state");
      }
    }
  private:
    const Playlist::Item::StateCallback& Callback;
    QPalette Palette;
  };
}

namespace Playlist
{
  namespace UI
  {
    TableViewItem::TableViewItem(QWidget& parent) : QItemDelegate(&parent)
    {
    }

    TableViewItem* TableViewItem::Create(QWidget& parent, const Item::StateCallback& callback)
    {
      return new TableViewItemImpl(parent, callback);
    }

    TableView::TableView(QWidget& parent) : QTableView(&parent)
    {
    }

    TableView* TableView::Create(QWidget& parent, const Item::StateCallback& callback, Playlist::Model::Ptr model)
    {
      REGISTER_METATYPE(Playlist::Model::IndexSetPtr);
      return new TableViewImpl(parent, callback, model);
    }
  }
}
