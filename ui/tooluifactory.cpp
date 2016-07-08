/*
  tooluifactory.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
  accordance with GammaRay Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tooluifactory.h"

#include "ui/propertywidget.h"
#include "ui/tools/objectinspector/objectinspectorwidget.h"
#include "ui/tools/objectinspector/propertiestab.h"
#include "ui/tools/objectinspector/methodstab.h"
#include "ui/tools/objectinspector/connectionstab.h"
#include "ui/tools/objectinspector/enumstab.h"
#include "ui/tools/objectinspector/classinfotab.h"
#include "ui/tools/objectinspector/propertiesextensionclient.h"
#include "ui/tools/objectinspector/methodsextensionclient.h"
#include "ui/tools/objectinspector/connectionsextensionclient.h"
#include "ui/tools/objectinspector/applicationattributetab.h"

#include <common/objectbroker.h>

using namespace GammaRay;

bool ToolUiFactory::s_ObjectFactoriesInitialized = false;

template<typename T>
static QObject *createExtension(const QString &name, QObject *parent)
{
    return new T(name, parent);
}

ToolUiFactory::ToolUiFactory()
{
    initUiObject();
}

ToolUiFactory::~ToolUiFactory()
{
}

QString ToolUiFactory::name() const
{
    return QString(); // in the common case this is provided via ProxyToolUIFactory
}

bool ToolUiFactory::remotingSupported() const
{
    return true;
}

void ToolUiFactory::initUi()
{

}

void ToolUiFactory::initUiObject()
{
    if (s_ObjectFactoriesInitialized)
        return;
    s_ObjectFactoriesInitialized = true;

    PropertyWidget::registerTab<PropertiesTab>(QStringLiteral("properties"),
                                               ObjectInspectorFactory::tr("Properties"),
                                               PropertyWidgetTabPriority::First);
    ObjectBroker::registerClientObjectFactoryCallback<PropertiesExtensionInterface *>(
        createExtension<PropertiesExtensionClient>);
    PropertyWidget::registerTab<MethodsTab>(QStringLiteral("methods"),
                                            ObjectInspectorFactory::tr("Methods"),
                                            PropertyWidgetTabPriority::Basic - 1);
    ObjectBroker::registerClientObjectFactoryCallback<MethodsExtensionInterface *>(
        createExtension<MethodsExtensionClient>);
    PropertyWidget::registerTab<ConnectionsTab>(QStringLiteral("connections"),
                                                ObjectInspectorFactory::tr("Connections"),
                                                PropertyWidgetTabPriority::Basic - 1);
    ObjectBroker::registerClientObjectFactoryCallback<ConnectionsExtensionInterface *>(
        createExtension<ConnectionsExtensionClient>);
    PropertyWidget::registerTab<EnumsTab>(QStringLiteral("enums"),
                                          ObjectInspectorFactory::tr("Enums"),
                                          PropertyWidgetTabPriority::Exotic - 1);
    PropertyWidget::registerTab<ClassInfoTab>(QStringLiteral("classInfo"),
                                              ObjectInspectorFactory::tr("Class Info"),
                                              PropertyWidgetTabPriority::Exotic - 1);
    PropertyWidget::registerTab<ApplicationAttributeTab>(
                QStringLiteral("applicationAttributes"),
                ObjectInspectorFactory::tr("Attributes"), PropertyWidgetTabPriority::Advanced);
}
