/*
    Copyright (C) 2010 Klarälvdalens Datakonsult AB,
        a KDAB Group company, info@kdab.net,
        author Stephen Kelly <stephen@kdab.com>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/

#include "klinkitemselectionmodel.h"

#include "kmodelindexproxymapper.h"

#include <QItemSelection>
#include <QTimer>
#include <QDebug>

class KLinkItemSelectionModelPrivate
{
public:
    KLinkItemSelectionModelPrivate(KLinkItemSelectionModel *proxySelectionModel, QAbstractItemModel *model,
                                   QItemSelectionModel *linkedItemSelectionModel)
        : q_ptr(proxySelectionModel),
          m_model(model),
          m_linkedItemSelectionModel(linkedItemSelectionModel),
          m_ignoreCurrentChanged(false),
          m_indexMapper(new KModelIndexProxyMapper(model, linkedItemSelectionModel->model(), proxySelectionModel)),
          m_queuedTimer(new QTimer(proxySelectionModel))
    {
        m_queuedTimer->setSingleShot(true);
        m_queuedTimer->setInterval(25);
    }

    Q_DECLARE_PUBLIC(KLinkItemSelectionModel)
    KLinkItemSelectionModel *const q_ptr;

    bool assertSelectionValid(const QItemSelection &selection) const
    {
        Q_FOREACH (const QItemSelectionRange &range, selection) {
            if (!range.isValid()) {
                qDebug() << selection;
            }
            Q_ASSERT(range.isValid());
        }
        return true;
    }

    void sourceSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void sourceCurrentChanged(const QModelIndex &current);
    void slotCurrentChanged(const QModelIndex &current);
    void slotSourceModelLayoutChanged();
    void slotHandleQueue();

    typedef QPair<QItemSelection, QItemSelection> SelectionChanges;
    typedef QVector<SelectionChanges> SelectionChangesList;

    QAbstractItemModel *const m_model;
    QItemSelectionModel *const m_linkedItemSelectionModel;
    bool m_ignoreCurrentChanged;
    KModelIndexProxyMapper *const m_indexMapper;
    QTimer *const m_queuedTimer;
    SelectionChangesList m_queuedChanges;
};

KLinkItemSelectionModel::KLinkItemSelectionModel(QAbstractItemModel *model, QItemSelectionModel *proxySelector, QObject *parent)
    : QItemSelectionModel(model, parent),
      d_ptr(new KLinkItemSelectionModelPrivate(this, model, proxySelector))
{
    Q_D(KLinkItemSelectionModel);
    connect(d->m_queuedTimer, SIGNAL(timeout()), SLOT(slotHandleQueue()));
    connect(model, SIGNAL(columnsInserted(QModelIndex,int,int)), SLOT(slotSourceModelLayoutChanged()));
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(slotSourceModelLayoutChanged()));
    connect(model, SIGNAL(layoutChanged()), SLOT(slotSourceModelLayoutChanged()));
    connect(model, SIGNAL(modelReset()), SLOT(slotSourceModelLayoutChanged()));
    connect(proxySelector, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), SLOT(sourceSelectionChanged(QItemSelection,QItemSelection)));
    connect(proxySelector, SIGNAL(currentChanged(QModelIndex,QModelIndex)), SLOT(sourceCurrentChanged(QModelIndex)));
    connect(this, SIGNAL(currentChanged(QModelIndex,QModelIndex)), SLOT(slotCurrentChanged(QModelIndex)));
}

KLinkItemSelectionModel::~KLinkItemSelectionModel()
{
    delete d_ptr;
}

void KLinkItemSelectionModel::select(const QModelIndex &index, QItemSelectionModel::SelectionFlags command)
{
    Q_D(KLinkItemSelectionModel);
    // When an item is removed, the current index is set to the top index in the model.
    // That causes a selectionChanged signal with a selection which we do not want.
    if (d->m_ignoreCurrentChanged) {
        return;
    }
    // Do *not* replace next line with: QItemSelectionModel::select(index, command)
    //
    // Doing so would end up calling KLinkItemSelectionModel::select(QItemSelection, QItemSelectionModel::SelectionFlags)
    //
    // This is because the code for QItemSelectionModel::select(QModelIndex, QItemSelectionModel::SelectionFlags) looks like this:
    // {
    //     QItemSelection selection(index, index);
    //     select(selection, command);
    // }
    // So it calls KLinkItemSelectionModel overload of
    // select(QItemSelection, QItemSelectionModel::SelectionFlags)
    //
    // When this happens and the selection flags include Toggle, it causes the
    // selection to be toggled twice.
    QItemSelectionModel::select(QItemSelection(index, index), command);
    if (index.isValid()) {
        d->m_linkedItemSelectionModel->select(d->m_indexMapper->mapSelectionLeftToRight(QItemSelection(index, index)), command);
    } else {
        d->m_linkedItemSelectionModel->clearSelection();
    }
}

// QAbstractProxyModel::mapSelectionFromSource creates invalid ranges to we filter
// those out manually in a loop. Hopefully fixed in Qt 4.7.2, so we ifdef it out.
// http://qt.gitorious.org/qt/qt/merge_requests/2474
// http://qt.gitorious.org/qt/qt/merge_requests/831
#if QT_VERSION < 0x040702
#define RANGE_FIX_HACK
#endif

#ifdef RANGE_FIX_HACK
static QItemSelection klink_removeInvalidRanges(const QItemSelection &selection)
{
    QItemSelection result;
    Q_FOREACH (const QItemSelectionRange &range, selection) {
        if (!range.isValid()) {
            continue;
        }
        result << range;
    }
    return result;
}
#endif

void KLinkItemSelectionModel::select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags command)
{
    Q_D(KLinkItemSelectionModel);
    d->m_ignoreCurrentChanged = true;
#ifdef RANGE_FIX_HACK
    QItemSelection _selection = klink_removeInvalidRanges(selection);
#else
    QItemSelection _selection = selection;
#endif
    QItemSelectionModel::select(_selection, command);
    Q_ASSERT(d->assertSelectionValid(_selection));
    QItemSelection mappedSelection = d->m_indexMapper->mapSelectionLeftToRight(_selection);
    Q_ASSERT(d->assertSelectionValid(mappedSelection));
    d->m_linkedItemSelectionModel->select(mappedSelection, command);
    d->m_ignoreCurrentChanged = false;
}

void KLinkItemSelectionModelPrivate::slotCurrentChanged(const QModelIndex &current)
{
    const QModelIndex mappedCurrent = m_indexMapper->mapLeftToRight(current);
    if (!mappedCurrent.isValid()) {
        return;
    }
    m_linkedItemSelectionModel->setCurrentIndex(mappedCurrent, QItemSelectionModel::NoUpdate);
}

void KLinkItemSelectionModelPrivate::slotSourceModelLayoutChanged()
{
    m_queuedTimer->start();
}

void KLinkItemSelectionModelPrivate::slotHandleQueue()
{
    Q_Q(KLinkItemSelectionModel);

    if (m_model->rowCount() == 0) {
        return;
    }

    foreach ( const SelectionChanges &changes, m_queuedChanges) {
        Q_ASSERT(assertSelectionValid(changes.first));
        Q_ASSERT(assertSelectionValid(changes.second));
        const QItemSelection mappedDeselection = m_indexMapper->mapSelectionRightToLeft(changes.second);
        const QItemSelection mappedSelection = m_indexMapper->mapSelectionRightToLeft(changes.first);

        q->QItemSelectionModel::select(mappedDeselection, QItemSelectionModel::Deselect);
        q->QItemSelectionModel::select(mappedSelection, QItemSelectionModel::Select);
    }

    m_queuedChanges.clear();
}

void KLinkItemSelectionModelPrivate::sourceSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_Q(KLinkItemSelectionModel);
#ifdef RANGE_FIX_HACK
    QItemSelection _selected = klink_removeInvalidRanges(selected);
    QItemSelection _deselected = klink_removeInvalidRanges(deselected);
#else
    QItemSelection _selected = selected;
    QItemSelection _deselected = deselected;
#endif
    Q_ASSERT(assertSelectionValid(_selected));
    Q_ASSERT(assertSelectionValid(_deselected));
    const QItemSelection mappedDeselection = m_indexMapper->mapSelectionRightToLeft(_deselected);
    const QItemSelection mappedSelection = m_indexMapper->mapSelectionRightToLeft(_selected);

    if (m_model->rowCount() == 0 && (!_selected.isEmpty() || !_deselected.isEmpty()) && (mappedSelection.isEmpty() || mappedDeselection.isEmpty())) {
        m_queuedChanges << qMakePair(_selected, _deselected);
    }
    else {
        q->QItemSelectionModel::select(mappedDeselection, QItemSelectionModel::Deselect);
        q->QItemSelectionModel::select(mappedSelection, QItemSelectionModel::Select);
    }
}

void KLinkItemSelectionModelPrivate::sourceCurrentChanged(const QModelIndex &current)
{
    Q_Q(KLinkItemSelectionModel);
    const QModelIndex mappedCurrent = m_indexMapper->mapRightToLeft(current);
    if (!mappedCurrent.isValid()) {
        return;
    }
    q->setCurrentIndex(mappedCurrent, QItemSelectionModel::NoUpdate);
}

#include "moc_klinkitemselectionmodel.cpp"
