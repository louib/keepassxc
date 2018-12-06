/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "keys/YkChallengeResponseKeyCLI.h"
#include "keys/drivers/YubiKey.h"

#include "core/Tools.h"
#include "crypto/CryptoHash.h"
#include "crypto/Random.h"

#include <QFile>
#include <QXmlStreamReader>
#include <QtCore/qglobal.h>

// FIXME Move somewhere else???
QUuid YkChallengeResponseKeyCLI::UUID("e092495c-e77d-498b-84a1-05ae0d955508");

YkChallengeResponseKeyCLI::YkChallengeResponseKeyCLI(
        int slot,
        QString messageInteraction,
        QString messageSuccess,
        QString messageFailure,
        FILE* errorDescriptor,
        FILE* outputDescriptor)
    : ChallengeResponseKey(UUID)
    , m_slot(slot)
    , m_messageInteraction(messageInteraction)
    , m_messageSuccess(messageSuccess)
    , m_messageFailure(messageFailure)
    , m_out(outputDescriptor)
    , m_err(errorDescriptor)
{
}

QByteArray YkChallengeResponseKeyCLI::rawKey() const
{
    return m_key;
}

/**
 * Assumes yubikey()->init() was called
 */
bool YkChallengeResponseKeyCLI::challenge(const QByteArray& challenge)
{
    return this->challenge(challenge, 2);
}

bool YkChallengeResponseKeyCLI::challenge(const QByteArray& challenge, unsigned int retries)
{
    QTextStream out(m_out, QIODevice::WriteOnly);
    // Change to Utils::STDERR when unit tested.
    QTextStream err(stderr, QIODevice::WriteOnly);
    do {
        --retries;

        out << m_messageInteraction << endl;
        YubiKey::ChallengeResult result = YubiKey::instance()->challenge(m_slot, true, challenge, m_key);
        if (result == YubiKey::SUCCESS) {
            out << m_messageSuccess << endl;
            return true;
        }
    } while (retries > 0);

    err << m_messageFailure << endl;
    return false;
}
