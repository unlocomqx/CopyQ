#include "qxtglobalshortcut.h"
/****************************************************************************
** Copyright (c) 2006 - 2011, the LibQxt project.
** See the Qxt AUTHORS file for a list of authors and copyright holders.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the LibQxt project nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
** <http://libqxt.org>  <foundation@libqxt.org>
*****************************************************************************/

#include "qxtglobalshortcut_p.h"
#include <QtDebug>
#include <common/log.h>
#include <scriptable/scriptable.h>
#include <QScriptEngine>
#include <script.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#ifndef Q_OS_MAC
int QxtGlobalShortcutPrivate::ref = 0;
#   if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
QAbstractEventDispatcher::EventFilter QxtGlobalShortcutPrivate::prevEventFilter = 0;
#   endif
#endif // Q_OS_MAC
QHash<QPair<quint32, quint32>, QxtGlobalShortcut *> QxtGlobalShortcutPrivate::shortcuts;

QxtGlobalShortcutPrivate::QxtGlobalShortcutPrivate()
        : enabled(true), key(Qt::Key(0)), mods(Qt::NoModifier), nativeKey(0), nativeMods(0), registered(false) {
#ifndef Q_OS_MAC
    if (ref == 0) {
#   if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        prevEventFilter = QAbstractEventDispatcher::instance()->setEventFilter(eventFilter);
#   else
        QAbstractEventDispatcher::instance()->installNativeEventFilter(this);
#endif
    }
    ++ref;
#endif // Q_OS_MAC
}

QxtGlobalShortcutPrivate::~QxtGlobalShortcutPrivate() {
    unsetShortcut();

#ifndef Q_OS_MAC
    --ref;
    if (ref == 0) {
        QAbstractEventDispatcher *ed = QAbstractEventDispatcher::instance();
        if (ed != nullptr) {
#   if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            ed->setEventFilter(prevEventFilter);
#   else
            ed->removeNativeEventFilter(this);
#   endif
        }
    }
#endif // Q_OS_MAC
}

bool QxtGlobalShortcutPrivate::setShortcut(const QKeySequence &shortcut) {
    unsetShortcut();

    const Qt::KeyboardModifiers allMods =
            Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
    const auto xkeyCode = static_cast<uint>( shortcut.isEmpty() ? 0 : shortcut[0] );
    // WORKAROUND: Qt has convert some keys to upper case which
    //             breaks some shortcuts on some keyboard layouts.
    const uint keyCode = QChar::toLower(xkeyCode & ~allMods);

    key = Qt::Key(keyCode);
    mods = Qt::KeyboardModifiers(xkeyCode & allMods);

    nativeKey = nativeKeycode(key);
    nativeMods = nativeModifiers(mods);

    registered = registerShortcut(nativeKey, nativeMods);
    if (registered)
        shortcuts.insert(qMakePair(nativeKey, nativeMods), &qxt_p());

    return registered;
}

bool QxtGlobalShortcutPrivate::unsetShortcut() {
    if (registered
        && shortcuts.value(qMakePair(nativeKey, nativeMods)) == &qxt_p()
        && unregisterShortcut(nativeKey, nativeMods)) {
        shortcuts.remove(qMakePair(nativeKey, nativeMods));
        registered = false;
        return true;
    }

    return false;
}

QxtGlobalShortcut *lastShortcut = nullptr;

void QxtGlobalShortcutPrivate::activateShortcut(quint32 nativeKey, quint32 nativeMods) {
    COPYQ_LOG(QString("shortcut %1 %2").arg(nativeKey).arg(nativeMods));
    QxtGlobalShortcut *shortcut = shortcuts.value(qMakePair(nativeKey, nativeMods));
    if (shortcut && shortcut->isEnabled()) {
        lastShortcut = shortcut;
        emit shortcut->activated(shortcut);
    }
}

void QxtGlobalShortcutPrivate::releaseShortcut() {
    COPYQ_LOG(QString("Pasting"));
    // QScriptEngine engine;
    // ScriptableProxy scriptableProxy(nullptr, nullptr);
    // Scriptable scriptable(&engine, &scriptableProxy);
    // scriptable.paste();
    // gScriptable->paste();
    // I couldn't access the Scriptable instance to use the paste() method
    // Instead I sent keycodes to simulate a paste action
    Display *display;
    unsigned int keycode_insert;
    unsigned int keycode_shift;
    display = XOpenDisplay(NULL);
    keycode_shift = XKeysymToKeycode(display, XK_Shift_L);
    keycode_insert = XKeysymToKeycode(display, XK_Insert);
    XTestFakeKeyEvent(display, keycode_shift, True, 0);
    XTestFakeKeyEvent(display, keycode_insert, True, 0);
    XTestFakeKeyEvent(display, keycode_insert, False, 0);
    XTestFakeKeyEvent(display, keycode_shift, False, 0);
    XFlush(display);
}

/*!
    \class QxtGlobalShortcut
    \inmodule QxtWidgets
    \brief The QxtGlobalShortcut class provides a global shortcut aka "hotkey".

    A global shortcut triggers even if the application is not active. This
    makes it easy to implement applications that react to certain shortcuts
    still if some other application is active or if the application is for
    example minimized to the system tray.

    Example usage:
    \code
    QxtGlobalShortcut* shortcut = new QxtGlobalShortcut(window);
    connect(shortcut, SIGNAL(activated()), window, SLOT(toggleVisibility()));
    shortcut->setShortcut(QKeySequence("Ctrl+Shift+F12"));
    \endcode

    \bold {Note:} Since Qxt 0.6 QxtGlobalShortcut no more requires QxtApplication.
 */

/*!
    \fn QxtGlobalShortcut::activated()

    This signal is emitted when the user types the shortcut's key sequence.

    \sa shortcut
 */

/*!
    Constructs a new QxtGlobalShortcut with \a parent.
 */
QxtGlobalShortcut::QxtGlobalShortcut(QObject *parent)
        : QObject(parent) {
    QXT_INIT_PRIVATE(QxtGlobalShortcut);
}

/*!
    Constructs a new QxtGlobalShortcut with \a shortcut and \a parent.
 */
QxtGlobalShortcut::QxtGlobalShortcut(const QKeySequence &shortcut, QObject *parent)
        : QObject(parent) {
    QXT_INIT_PRIVATE(QxtGlobalShortcut);
    setShortcut(shortcut);
}

/*!
    Destructs the QxtGlobalShortcut.
 */
QxtGlobalShortcut::~QxtGlobalShortcut() {
}

/*!
    \property QxtGlobalShortcut::shortcut
    \brief the shortcut key sequence

    Notice that corresponding key press and release events are not
    delivered for registered global shortcuts even if they are disabled.
    Also, comma separated key sequences are not supported.
    Only the first part is used:

    \code
    qxtShortcut->setShortcut(QKeySequence("Ctrl+Alt+A,Ctrl+Alt+B"));
    Q_ASSERT(qxtShortcut->shortcut() == QKeySequence("Ctrl+Alt+A"));
    \endcode
 */
QKeySequence QxtGlobalShortcut::shortcut() const {
    return QKeySequence(static_cast<int>(qxt_d().key | qxt_d().mods));
}

bool QxtGlobalShortcut::setShortcut(const QKeySequence &shortcut) {
    return qxt_d().setShortcut(shortcut);
}

/*!
    \property QxtGlobalShortcut::enabled
    \brief whether the shortcut is enabled

    A disabled shortcut does not get activated.

    The default value is \c true.

    \sa setDisabled()
 */
bool QxtGlobalShortcut::isEnabled() const {
    return qxt_d().enabled;
}

bool QxtGlobalShortcut::isValid() const {
    return qxt_d().registered;
}

void QxtGlobalShortcut::setEnabled(bool enabled) {
    qxt_d().enabled = enabled;
}

/*!
    Sets the shortcut \a disabled.

    \sa enabled
 */
void QxtGlobalShortcut::setDisabled(bool disabled) {
    qxt_d().enabled = !disabled;
}
