/*
 * WarriorLords Tiled Plugin
 * Copyright 2010, Jaderamiso <jaderamiso@gmail.com>
 * Copyright 2011, Stefan Beller <stefanbeller@googlemail.com>
 * Copyright 2012, Manu Evans <turkeyman@gmail.com>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WARRIORLORDSPLUGIN_H
#define WARRIORLORDSPLUGIN_H

#include "warriorlords_global.h"

#include "mapreaderinterface.h"
#include "mapwriterinterface.h"

#include <QObject>

namespace wl {

class WARRIORLORDSSHARED_EXPORT WarriorLordsPlugin
        : public QObject
        , public Tiled::MapReaderInterface
        , public Tiled::MapWriterInterface
{
    Q_OBJECT
    Q_INTERFACES(Tiled::MapReaderInterface)
    Q_INTERFACES(Tiled::MapWriterInterface)

public:
    WarriorLordsPlugin();

    // MapReaderInterface
    Tiled::Map *read(const QString &fileName);
    bool supportsFile(const QString &fileName) const;

    // MapWriterInterface
    bool write(const Tiled::Map *map, const QString &fileName);
    QString nameFilter() const;
    QString errorString() const;

private:
    bool checkOneLayerWithName(const Tiled::Map *map, const QString &name);

    QString mError;
};

} // namespace wl

#endif // WARRIORLORDSPLUGIN_H
