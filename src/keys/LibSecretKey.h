#ifndef KEEPASSX_LIBSECRETKEY_H
#define KEEPASSX_LIBSECRETKEY_H

#include "keys/Key.h"
#include <QString>
#include <QByteArray>

#include <libsecret/secret.h>

class LibSecretKey : public Key
{
public:
    LibSecretKey(QString);

    QByteArray rawKey() const;
    LibSecretKey* clone() const;
	QString loadKey();
	static QString storeKey(QByteArray key, QString keyName);

private:
    QByteArray theKey;

    static const SecretSchema* getSchema();
	QString keyName;
};

#endif // KEEPASSX_LIBSECRETKEY_H
