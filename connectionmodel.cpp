#include "connectionmodel.h"
#include "util.h"

#include <QDebug>
#include <QMetaMethod>
#include <QMetaObject>

using namespace Endoscope;

static QMetaMethod methodForObject( QObject *obj, const QByteArray &signature ) 
{
  if ( !obj || signature.isEmpty() )
    return QMetaMethod();
  const QMetaObject *mo = obj->metaObject();
  const int methodIndex = mo->indexOfMethod( signature.mid( 1 ) );
  if ( methodIndex < 0 )
    return QMetaMethod();
  return mo->method( methodIndex );
}

ConnectionModel::ConnectionModel(QObject* parent): QAbstractTableModel(parent)
{
}

void ConnectionModel::connectionAdded(QObject* sender, const char* signal, QObject* receiver, const char* method, Qt::ConnectionType type)
{
  if ( sender == this || receiver == this )
    return;
  beginInsertRows( QModelIndex(), m_connections.size(), m_connections.size() );
  Connection c;
  c.sender = sender;
  c.rawSender = sender;
  c.signal = QMetaObject::normalizedSignature( signal );
  c.receiver = receiver;
  c.rawReceiver = receiver;
  c.method = QMetaObject::normalizedSignature( method );
  c.type = type;
  
  // check if that's actually a valid connection
  const QMetaMethod signalMethod = methodForObject( sender, c.signal );
  const QMetaMethod receiverMethod = methodForObject( receiver, c.method );
  if ( signalMethod.methodType() == QMetaMethod::Signal && 
    ( receiverMethod.methodType() == QMetaMethod::Slot || receiverMethod.methodType() == QMetaMethod::Signal ) ) 
  {
    c.valid = QMetaObject::checkConnectArgs( c.signal, c.method );
  } else {
    c.valid = false;
  }
  
  m_connections.push_back( c );
  endInsertRows();
}

void ConnectionModel::connectionRemoved(QObject* sender, const char* signal, QObject* receiver, const char* method)
{
  if ( sender == this || receiver == this )
    return;

  QByteArray normalizedSignal, normalizedMethod;
  if ( signal )
    normalizedSignal = QMetaObject::normalizedSignature( signal );
  if ( method )
    normalizedMethod = QMetaObject::normalizedSignature( method );

  for ( int i = 0; i < m_connections.size(); ) {
    const Connection &con = m_connections.at( i );
    if ( (sender == 0 || con.rawSender == sender)
      && (signal == 0 || con.signal == normalizedSignal)
      && (receiver == 0 || con.rawReceiver == receiver)
      && (method == 0 || con.method == normalizedMethod) )
    {
      beginRemoveRows( QModelIndex(), i, i );
      m_connections.remove( i );
      endRemoveRows();
    } else {
      ++i;
    }
  }
}

QVariant ConnectionModel::data(const QModelIndex& index, int role) const
{
  if ( !index.isValid() || index.row() < 0 || index.row() >= m_connections.size() )
    return QVariant();

  const Connection con = m_connections.at( index.row() );
  if ( role == Qt::DisplayRole ) {
    if ( index.column() == 0 ) {
      if ( con.sender )
        return Util::displayString( con.sender.data() );
      else
        return QLatin1String( "<destroyed>" );
    }
    if ( index.column() == 1 )
      return con.signal.mid( 1 );
    if ( index.column() == 2 ) {
      if ( con.receiver )
        return Util::displayString( con.receiver.data() );
      else
        return QLatin1String( "<destroyed>" );
    }
    if ( index.column() == 3 )
      return con.method.mid( 1 );
    if ( index.column() == 4 ) {
      switch ( con.type ) {
        case Qt::AutoCompatConnection: return QLatin1String( "AutoCompatConnection" );
        case Qt::AutoConnection: return QLatin1String( "AutoConnection" );
        case Qt::BlockingQueuedConnection: return QLatin1String( "BlockingQueuedConnection" );
        case Qt::DirectConnection: return QLatin1String( "DirectConnection" );
        case Qt::QueuedConnection: return QLatin1String( "QueuedConnection" );
        case Qt::UniqueConnection: return QLatin1String( "UniqueConnection" );
        default: return tr( "Unknown connection type: %1" ).arg( con.type );
      }
    }
  } else if ( role == SenderRole ) {
    return QVariant::fromValue( con.sender.data() );
  } else if ( role == ReceiverRole ) {
    return QVariant::fromValue( con.receiver.data() );
  } else if ( role == Qt::ForegroundRole ) {
    if ( !con.valid )
      return Qt::red;
  }
  return QVariant();
}

QVariant ConnectionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
    switch ( section ) {
      case 0: return tr( "Sender" );
      case 1: return tr( "Signal" );
      case 2: return tr( "Receiver" );
      case 3: return tr( "Method" );
      case 4: return tr( "Connection Type" );
    }
  }
  return QAbstractItemModel::headerData(section, orientation, role);
}

int ConnectionModel::columnCount(const QModelIndex& parent) const
{
  Q_UNUSED( parent );
  return 5;
}

int ConnectionModel::rowCount(const QModelIndex& parent) const
{
  if ( parent.isValid() )
    return 0;
  return m_connections.size();
}

#include "connectionmodel.moc"
