#include "databaseitem.h"
#include "namespaceitem.h"
#include "keyitem.h"
#include "connections-tree/iconproxy.h"
#include <typeinfo>
#include <functional>
#include <QDebug>
#include <QMenu>

using namespace ConnectionsTree;

DatabaseItem::DatabaseItem(const QString& displayName,
                           unsigned int index, int keysCount,
                           QSharedPointer<Operations> operations,
                           const TreeItem* parent)
    : m_name(displayName),
      m_index(index),
      m_keysCount(keysCount),
      m_locked(false),
      m_operations(operations),
      m_parent(parent)
{    
    QObject::connect(&m_keysLoadingWatcher, SIGNAL(finished()), this, SLOT(onKeysRendered()));
}

DatabaseItem::~DatabaseItem()
{
    m_parent = nullptr;
}

QString DatabaseItem::getDisplayName() const
{
    return m_keys.isEmpty()? m_name : QString("%1 (%2/%3)").arg(m_name).arg(childCount()).arg(m_keysCount);
}

QIcon DatabaseItem::getIcon() const
{
    if (m_locked)    return IconProxy::instance()->get(":/images/wait.png");
    return IconProxy::instance()->get(":/images/db.png");
}

QList<QSharedPointer<TreeItem> > DatabaseItem::getAllChilds() const
{
    return m_keys;
}

uint DatabaseItem::childCount() const
{
    return m_keys.size();
}

QSharedPointer<TreeItem> DatabaseItem::child(uint row) const
{
    if (row < childCount())
        return m_keys.at(row);

    return QSharedPointer<TreeItem>();
}

const TreeItem *DatabaseItem::parent() const
{
    return m_parent;
}

bool DatabaseItem::onClick(ParentView&)
{    
    loadKeys();
    return true;
}

void DatabaseItem::onWheelClick(TreeItem::ParentView&)
{    
}

QSharedPointer<QMenu> DatabaseItem::getContextMenu(TreeItem::ParentView& treeView)
{
    Q_UNUSED(treeView);
    QSharedPointer<QMenu> menu(new QMenu());

    //new key action
    QAction* newKey = new QAction(QIcon(":/images/add.png"), "Add new key", menu.data());
    QObject::connect(newKey, &QAction::triggered, this, [this]() { m_operations->openNewKeyDialog(m_index); });
    menu->addAction(newKey);
    menu->addSeparator();

    //reload action
    QAction* reload = new QAction(QIcon(":/images/refreshdb.png"), "Reload", menu.data());
    QObject::connect(reload, &QAction::triggered, this, [this] { this->reload(); });
    menu->addAction(reload);

    return menu;
}

void DatabaseItem::loadKeys()
{
    if (m_keys.size() > 0) {
        emit keysLoaded(m_index);
        return;
    }

    m_locked = true;
    emit updateIcon(m_index);

    m_operations->getDatabaseKeys(m_index, [this](const Operations::RawKeysList& rawKeys) {

        qDebug() << "Keys: " << rawKeys.size();

        if (rawKeys.size() == 0) {
            m_locked = false;
            return;
        }        

        QRegExp filter;
        QString separator(m_operations->getNamespaceSeparator());        

        QFuture<QList<QSharedPointer<TreeItem>>> keysLoadingResult =
                QtConcurrent::run(&m_keysRenderer,&KeysTreeRenderer::renderKeys,
                                  m_operations, rawKeys, filter, separator, this);

        m_keysLoadingWatcher.setFuture(keysLoadingResult);

    });
}

int DatabaseItem::getIndex() const
{
    return m_index;
}

void DatabaseItem::onKeysRendered()
{
    m_keys = m_keysLoadingWatcher.result();
    m_locked = false;
    emit keysLoaded(m_index);
}

void DatabaseItem::unload()
{
    if (m_keys.size() == 0)
        return;

    m_locked = true;
    emit unloadStarted(m_index);

    m_keys.clear();
    m_locked = false;
}

void DatabaseItem::reload()
{
    unload();
    loadKeys();
}

QList<QSharedPointer<TreeItem> >
DatabaseItem::KeysTreeRenderer::renderKeys(QSharedPointer<Operations> operations,
                                           Operations::RawKeysList keys,
                                           QRegExp filter,
                                           QString namespaceSeparator,
                                           const DatabaseItem* parent)
{
    //init
    keys.sort();
    QList<QSharedPointer<TreeItem>> result;

    //render
    for (QVariant key : keys) {

        QString rawKey = key.toString();

        //if filter enabled - skip keys
        if (!filter.isEmpty() && !rawKey.contains(filter)) {
            continue;
        }

        renderNamaspacedKey(QSharedPointer<NamespaceItem>(),
                            rawKey, rawKey, operations,
                            namespaceSeparator, result, parent);
    }

    return result;
}

void DatabaseItem::KeysTreeRenderer::renderNamaspacedKey(QSharedPointer<NamespaceItem> currItem,
                                                         const QString &notProcessedKeyPart,
                                                         const QString &fullKey,
                                                         QSharedPointer<Operations> m_operations,                                                         
                                                         const QString& m_namespaceSeparator,
                                                         QList<QSharedPointer<TreeItem>>& m_result, const DatabaseItem *db)
{
    const TreeItem* currentParent = (currItem.isNull())? static_cast<const TreeItem*>(db) : currItem.data();

    if (!notProcessedKeyPart.contains(m_namespaceSeparator) || m_namespaceSeparator.isEmpty()) {

        QSharedPointer<KeyItem> newKey(
                    (new KeyItem(fullKey, db->getIndex(), m_operations, currentParent))
                    );

        if (currItem.isNull()) m_result.push_back(newKey);
        else currItem->append(newKey);

        return;
    }

    int indexOfNaspaceSeparator = notProcessedKeyPart.indexOf(m_namespaceSeparator);

    QString firstNamespaceName = notProcessedKeyPart.mid(0, indexOfNaspaceSeparator);

    QSharedPointer<NamespaceItem> namespaceItem;
    int size = (currItem.isNull())? m_result.size() : currItem->childCount();

    for (int i=0; i < size; ++i)
    {
        QSharedPointer<TreeItem> child = (currItem.isNull())? m_result[i] : currItem->child(i);

        if (child->getDisplayName() == firstNamespaceName
                && typeid(NamespaceItem)==typeid(*child)) {

                namespaceItem =  qSharedPointerCast<NamespaceItem>(child);
                break;
        }
    }


    if (namespaceItem.isNull()) {
        namespaceItem = QSharedPointer<NamespaceItem>(new NamespaceItem(firstNamespaceName, m_operations, currentParent));

        if (currItem.isNull()) m_result.push_back(namespaceItem);
        else currItem->append(namespaceItem);
    }

    renderNamaspacedKey(namespaceItem, notProcessedKeyPart.mid(indexOfNaspaceSeparator+m_namespaceSeparator.length()),
                        fullKey, m_operations, m_namespaceSeparator, m_result, db);
}
