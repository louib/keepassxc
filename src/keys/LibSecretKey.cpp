#include "keys/LibSecretKey.h"
#include "crypto/Random.h"

#include <QDebug>

LibSecretKey::LibSecretKey(QString keyName)
{
	this->keyName = keyName;
}

QByteArray LibSecretKey::rawKey() const {
    return theKey;
}

LibSecretKey* LibSecretKey::clone() const {
    return new LibSecretKey(*this);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
inline const SecretSchema* LibSecretKey::getSchema() {
    static const SecretSchema _schema = {
        "org.keepassx.keystorage",
        SECRET_SCHEMA_NONE,
        {
			{  "string", SECRET_SCHEMA_ATTRIBUTE_STRING },
        }
    };
    return &_schema;
}
#pragma GCC diagnostic pop

QString LibSecretKey::storeKey(QByteArray key, QString keyName) {

    GError* error = nullptr;
    secret_password_store_sync(getSchema(),
			                   SECRET_COLLECTION_DEFAULT,
							   keyName.toLatin1(),
							   key.toBase64(),
                               nullptr,
							   &error,
                               nullptr);

	if (error == nullptr) {
		return QString();
    }

	QString errorMessage = QString(error->message);
	g_error_free(error);
	return errorMessage;
}

QString LibSecretKey::loadKey() {

    GError* error = nullptr;
	gchar* key = secret_password_lookup_sync(getSchema(),
											 NULL,
											 &error,
											 this->keyName.toLatin1(),
											 NULL);
    if (error != nullptr) {
        QString errorMessage = QString(error->message);
        g_error_free(error);
        return errorMessage;
    }

    if (key == nullptr) {
        return QString("The key was not found");
    }

	theKey = QByteArray::fromBase64(key);
	secret_password_free(key);
	return QString();
}
