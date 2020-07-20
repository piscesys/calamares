/* === This file is part of Calamares - <https://github.com/calamares> ===
 *
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *   License-Filename: LICENSE
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

#ifndef LOCALE_CONFIG_H
#define LOCALE_CONFIG_H

#include "Job.h"
#include "locale/TimeZone.h"

#include <QObject>

#include <memory>

class Config : public QObject
{
    Q_OBJECT
    Q_PROPERTY( const QStringList& supportedLocales READ supportedLocales CONSTANT FINAL )
    Q_PROPERTY( CalamaresUtils::Locale::CStringListModel* zonesModel READ zonesModel CONSTANT FINAL )
    Q_PROPERTY( CalamaresUtils::Locale::CStringListModel* regionModel READ regionModel CONSTANT FINAL )
    Q_PROPERTY( const CalamaresUtils::Locale::CStringPairList& timezoneData READ timezoneData CONSTANT FINAL )
    Q_PROPERTY( const CalamaresUtils::Locale::TZZone* currentLocation READ currentLocation WRITE setCurrentLocation NOTIFY currentLocationChanged )

public:
    Config( QObject* parent = nullptr );
    ~Config();

    void setConfigurationMap( const QVariantMap& );
    Calamares::JobList createJobs();

public Q_SLOTS:
    const QStringList& supportedLocales() const { return m_localeGenLines; }
    CalamaresUtils::Locale::CStringListModel* regionModel() const { return m_regionModel.get(); }
    CalamaresUtils::Locale::CStringListModel* zonesModel() const { return m_zonesModel.get(); }
    // Underlying data for the models
    const CalamaresUtils::Locale::CStringPairList& timezoneData() const;

    /** @brief Sets a location by name
     *
     * @p region should be "America" or the like, while @p zone
     * names a zone within that region.
     */
    void setCurrentLocation( const QString& region, const QString& zone );
    /** @brief Sets a location by pointer
     *
     * Pointer should be within the same model as the widget uses.
     */
    void setCurrentLocation( const CalamaresUtils::Locale::TZZone* location );

    const CalamaresUtils::Locale::TZZone* currentLocation() const { return m_currentLocation; }

signals:
    void currentLocationChanged( const CalamaresUtils::Locale::TZZone* location );

private:
    /// A list of supported locale identifiers (e.g. "en_US.UTF-8")
    QStringList m_localeGenLines;

    /// The regions (America, Asia, Europe ..)
    std::unique_ptr< CalamaresUtils::Locale::CStringListModel > m_regionModel;
    /// The zones for the current region (e.g. America/New_York)
    std::unique_ptr< CalamaresUtils::Locale::CStringListModel > m_zonesModel;

    /// The location, points into the timezone data
    const CalamaresUtils::Locale::TZZone* m_currentLocation = nullptr;

};


#endif
