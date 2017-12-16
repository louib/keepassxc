#ifndef KEEPASSX_LIBSECRETKEY_H
#define KEEPASSX_LIBSECRETKEY_H

#include "keys/Key.h"
#include <QString>

#include <libsecret/secret.h>

class LibSecretKey : public Key
{
public:
    LibSecretKey(QString);

    QByteArray rawKey() const;
    LibSecretKey* clone() const;
	QString loadKey();

private:
    QByteArray theKey;

    static const SecretSchema* getSchema();
	QString keyName;
	static QString storeKey(QByteArray key, QString keyName);
};

#endif // KEEPASSX_LIBSECRETKEY_H
