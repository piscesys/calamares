/* === This file is part of Calamares - <https://github.com/calamares> ===
 *
 *   Copyright 2014, Aurélien Gâteau <agateau@kde.org>
 *   Copyright 2015-2016, Teo Mrnjavac <teo@kde.org>
 *   Copyright 2017, 2019-2020, Adriaan de Groot <groot@kde.org>
 *
 *   Calamares is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Calamares is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Calamares. If not, see <http://www.gnu.org/licenses/>.
 */

#include "jobs/FillGlobalStorageJob.h"

#include "core/KPMHelpers.h"
#include "core/PartitionInfo.h"
#include "core/PartitionIterator.h"

#include "Branding.h"
#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"

// KPMcore
#include <core/device.h>
#include <core/partition.h>
#include <fs/filesystem.h>
#include <fs/luks.h>

// Qt
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

using KPMHelpers::untranslatedFS;
using KPMHelpers::userVisibleFS;

typedef QHash< QString, QString > UuidForPartitionHash;

static UuidForPartitionHash
findPartitionUuids( QList< Device* > devices )
{
    UuidForPartitionHash hash;
    foreach ( Device* device, devices )
    {
        for ( auto it = PartitionIterator::begin( device ); it != PartitionIterator::end( device ); ++it )
        {
            Partition* p = *it;
            QString path = p->partitionPath();
            QString uuid = p->fileSystem().readUUID( p->partitionPath() );
            hash.insert( path, uuid );
        }
    }

    if ( hash.isEmpty() )
    {
        cDebug() << "No UUIDs found for existing partitions.";
    }
    return hash;
}


static QString
getLuksUuid( const QString& path )
{
    QProcess process;
    process.setProgram( "cryptsetup" );
    process.setArguments( { "luksUUID", path } );
    process.start();
    process.waitForFinished();
    if ( process.exitStatus() != QProcess::NormalExit || process.exitCode() )
    {
        return QString();
    }
    QString uuid = QString::fromLocal8Bit( process.readAllStandardOutput() ).trimmed();
    return uuid;
}


static QVariant
mapForPartition( Partition* partition, const QString& uuid )
{
    QVariantMap map;
    map[ "device" ] = partition->partitionPath();
    map[ "mountPoint" ] = PartitionInfo::mountPoint( partition );
    map[ "fsName" ] = userVisibleFS( partition->fileSystem() );
    map[ "fs" ] = untranslatedFS( partition->fileSystem() );
    if ( partition->fileSystem().type() == FileSystem::Luks
         && dynamic_cast< FS::luks& >( partition->fileSystem() ).innerFS() )
    {
        map[ "fs" ] = untranslatedFS( dynamic_cast< FS::luks& >( partition->fileSystem() ).innerFS() );
    }
    map[ "uuid" ] = uuid;
    map[ "new" ] = partition->state() == Partition::StateNew;

    // Debugging for inside the loop in createPartitionList(),
    // so indent a bit
    Logger::CDebug deb;
    using TR = Logger::DebugRow< const char* const, const QString& >;
    deb << Logger::SubEntry << "mapping for" << partition->partitionPath() << partition->deviceNode()
        << TR( "mtpoint:", PartitionInfo::mountPoint( partition ) ) << TR( "fs:", map[ "fs" ].toString() )
        << TR( "fsName", map[ "fsName" ].toString() ) << TR( "uuid", uuid );

    if ( partition->roles().has( PartitionRole::Luks ) )
    {
        const FileSystem& fsRef = partition->fileSystem();
        const FS::luks* luksFs = dynamic_cast< const FS::luks* >( &fsRef );
        if ( luksFs )
        {
            map[ "luksMapperName" ] = luksFs->mapperName().split( "/" ).last();
            map[ "luksUuid" ] = getLuksUuid( partition->partitionPath() );
            map[ "luksPassphrase" ] = luksFs->passphrase();
            deb << TR( "luksMapperName:", map[ "luksMapperName" ].toString() );
        }
    }

    return map;
}

FillGlobalStorageJob::FillGlobalStorageJob( QList< Device* > devices, const QString& bootLoaderPath )
    : m_devices( devices )
    , m_bootLoaderPath( bootLoaderPath )
{
}

QString
FillGlobalStorageJob::prettyName() const
{
    return tr( "Set partition information" );
}


QString
FillGlobalStorageJob::prettyDescription() const
{
    QStringList lines;

    const auto partitionList = createPartitionList().toList();
    for ( const QVariant& partitionItem : partitionList )
    {
        if ( partitionItem.type() == QVariant::Map )
        {
            QVariantMap partitionMap = partitionItem.toMap();
            QString path = partitionMap.value( "device" ).toString();
            QString mountPoint = partitionMap.value( "mountPoint" ).toString();
            QString fsType = partitionMap.value( "fs" ).toString();
            if ( mountPoint.isEmpty() || fsType.isEmpty() )
            {
                continue;
            }
            if ( path.isEmpty() )
            {
                if ( mountPoint == "/" )
                {
                    lines.append( tr( "Install %1 on <strong>new</strong> %2 system partition." )
                                      .arg( *Calamares::Branding::ShortProductName )
                                      .arg( fsType ) );
                }
                else
                {
                    lines.append( tr( "Set up <strong>new</strong> %2 partition with mount point "
                                      "<strong>%1</strong>." )
                                      .arg( mountPoint )
                                      .arg( fsType ) );
                }
            }
            else
            {
                if ( mountPoint == "/" )
                {
                    lines.append( tr( "Install %2 on %3 system partition <strong>%1</strong>." )
                                      .arg( path )
                                      .arg( *Calamares::Branding::ShortProductName )
                                      .arg( fsType ) );
                }
                else
                {
                    lines.append( tr( "Set up %3 partition <strong>%1</strong> with mount point "
                                      "<strong>%2</strong>." )
                                      .arg( path )
                                      .arg( mountPoint )
                                      .arg( fsType ) );
                }
            }
        }
    }

    QVariant bootloaderMap = createBootLoaderMap();
    if ( !m_bootLoaderPath.isEmpty() )
    {
        lines.append( tr( "Install boot loader on <strong>%1</strong>." ).arg( m_bootLoaderPath ) );
    }
    return lines.join( "<br/>" );
}


QString
FillGlobalStorageJob::prettyStatusMessage() const
{
    return tr( "Setting up mount points." );
}

Calamares::JobResult
FillGlobalStorageJob::exec()
{
    Calamares::GlobalStorage* storage = Calamares::JobQueue::instance()->globalStorage();
    storage->insert( "partitions", createPartitionList() );
    cDebug() << "Saving partition information map to GlobalStorage[\"partitions\"]";
    if ( !m_bootLoaderPath.isEmpty() )
    {
        QVariant var = createBootLoaderMap();
        if ( !var.isValid() )
        {
            cDebug() << "Failed to find path for boot loader";
        }
        cDebug() << "FillGlobalStorageJob writing bootLoader path:" << var;
        storage->insert( "bootLoader", var );
    }
    else
    {
        cDebug() << "FillGlobalStorageJob writing empty bootLoader value";
        storage->insert( "bootLoader", QVariant() );
    }
    return Calamares::JobResult::ok();
}

QVariant
FillGlobalStorageJob::createPartitionList() const
{
    UuidForPartitionHash hash = findPartitionUuids( m_devices );
    QVariantList lst;
    cDebug() << "Building partition information map";
    for ( auto device : m_devices )
    {
        cDebug() << Logger::SubEntry << "partitions on" << device->deviceNode();
        for ( auto it = PartitionIterator::begin( device ); it != PartitionIterator::end( device ); ++it )
        {
            // Debug-logging is done when creating the map
            lst << mapForPartition( *it, hash.value( ( *it )->partitionPath() ) );
        }
    }
    return lst;
}

QVariant
FillGlobalStorageJob::createBootLoaderMap() const
{
    QVariantMap map;
    QString path = m_bootLoaderPath;
    if ( !path.startsWith( "/dev/" ) )
    {
        Partition* partition = KPMHelpers::findPartitionByMountPoint( m_devices, path );
        if ( !partition )
        {
            return QVariant();
        }
        path = partition->partitionPath();
    }
    map[ "installPath" ] = path;
    return map;
}
