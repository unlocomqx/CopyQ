/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "gui/notificationdaemon.h"

#include "common/common.h"
#include "common/display.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "gui/notification.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QPixmap>
#include <QPoint>
#include <QVariant>

namespace {

const int notificationMarginPoints = 10;

int notificationMargin()
{
    return pointsToPixels(notificationMarginPoints);
}

} // namespace

NotificationDaemon::NotificationDaemon(QObject *parent)
    : QObject(parent)
    , m_position(BottomRight)
    , m_notifications()
    , m_opacity(1.0)
    , m_horizontalOffsetPoints(0)
    , m_verticalOffsetPoints(0)
    , m_maximumWidthPoints(300)
    , m_maximumHeightPoints(100)
{
    initSingleShotTimer( &m_timerUpdate, 100, this, SLOT(doUpdateNotifications()) );
}

void NotificationDaemon::setPosition(NotificationDaemon::Position position)
{
    m_position = position;
}

void NotificationDaemon::setOffset(int horizontalPoints, int verticalPoints)
{
    m_horizontalOffsetPoints = horizontalPoints;
    m_verticalOffsetPoints = verticalPoints;
}

void NotificationDaemon::setMaximumSize(int maximumWidthPoints, int maximumHeightPoints)
{
    m_maximumWidthPoints = maximumWidthPoints;
    m_maximumHeightPoints = maximumHeightPoints;
}

void NotificationDaemon::updateNotifications()
{
    if ( !m_timerUpdate.isActive() )
        m_timerUpdate.start();
}

void NotificationDaemon::setNotificationOpacity(qreal opacity)
{
    m_opacity = opacity;
}

void NotificationDaemon::setNotificationStyleSheet(const QString &styleSheet)
{
    m_styleSheet = styleSheet;
}

void NotificationDaemon::removeNotification(const QString &id)
{
    Notification *notification = findNotification(id);
    if (notification)
        onNotificationClose(notification);
}

void NotificationDaemon::onNotificationClose(Notification *notification)
{
    for (auto it = std::begin(m_notifications); it != std::end(m_notifications); ++it) {
        if (it->notification == notification) {
            m_notifications.erase(it);
            break;
        }
    }

    notification->deleteLater();
    updateNotifications();
}

void NotificationDaemon::doUpdateNotifications()
{
    const QRect screen = QApplication::desktop()->screenGeometry();

    int y = (m_position & Top) ? offsetY() : screen.bottom() - offsetY();

    for (auto &notificationData : m_notifications) {
        auto notification = notificationData.notification;
        notification->setOpacity(m_opacity);
        notification->setStyleSheet(m_styleSheet);
        notification->updateIcon();
        notification->adjust();
        notification->setMaximumSize( pointsToPixels(m_maximumWidthPoints), pointsToPixels(m_maximumHeightPoints) );

        int x;
        if (m_position & Left)
            x = offsetX();
        else if (m_position & Right)
            x = screen.right() - notification->width() - offsetX();
        else
            x = screen.right() / 2 - notification->width() / 2;

        if (m_position & Bottom)
            y -= notification->height();

        // I centered the tooltip to bring it to field of view
        x = (screen.width() - notification->width()) / 2;
        y = (screen.height() - notification->height()) / 2;

        notification->move(x, y);

        if (m_position & Top)
            y += notification->height() + notificationMargin();
        else
            y -= notificationMargin();

        notification->show();
    }
}

Notification *NotificationDaemon::findNotification(const QString &id)
{
    for (auto &notificationData : m_notifications) {
        if (notificationData.id == id)
            return notificationData.notification;
    }

    return nullptr;
}

Notification *NotificationDaemon::createNotification(const QString &id)
{
    Notification *notification = nullptr;
    if ( !id.isEmpty() )
        notification = findNotification(id);

    if (notification == nullptr) {
        notification = new Notification();
        connect(this, SIGNAL(destroyed()), notification, SLOT(deleteLater()));
        connect( notification, SIGNAL(closeNotification(Notification*)),
                 this, SLOT(onNotificationClose(Notification*)) );
        connect( notification, SIGNAL(buttonClicked(NotificationButton)),
                 this, SIGNAL(notificationButtonClicked(NotificationButton)) );

        m_notifications.append(NotificationData{id, notification});
    }

    updateNotifications();

    return notification;
}

int NotificationDaemon::offsetX() const
{
    return pointsToPixels(m_horizontalOffsetPoints);
}

int NotificationDaemon::offsetY() const
{
    return pointsToPixels(m_verticalOffsetPoints);
}
