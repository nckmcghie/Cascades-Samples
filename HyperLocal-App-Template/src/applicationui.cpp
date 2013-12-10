/* Copyright (c) 2013 BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "applicationui.hpp"
#include "bbm/BBMHandler.hpp"
#include "activeFrameQML.h"
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/LocaleHandler>
#include <bb/system/InvokeRequest>
#include <bb/data/JsonDataAccess>

using namespace bb::cascades;

ApplicationUI::ApplicationUI(bb::cascades::Application *app) :
		QObject(app) {

	// prepare the localization
	m_pTranslator = new QTranslator(this);
	m_pLocaleHandler = new LocaleHandler(this);
	m_invokeManager = new bb::system::InvokeManager(this);

	// translation
	if (!QObject::connect(m_pLocaleHandler, SIGNAL(systemLanguageChanged()),
			this, SLOT(onSystemLanguageChanged()))) {
		// This is an abnormal situation! Something went wrong!
		// Add own code to recover here
		qWarning() << "Recovering from a failed connect()";
	}
	onSystemLanguageChanged();

	// we use a qtimer to delay the start of the app, this allows the splash
	// screen to display for a set amount of time - 1.7 seconds / 1700ms
	QTimer::singleShot(1700, this, SLOT(runThisJazz()));
}

// executed by the qTimer above
void ApplicationUI::runThisJazz() {
	bb::cascades::Application *app = Application::instance();
	bb::data::JsonDataAccess jda;

	// load our app settings from settins.json file
	QVariantMap appSettings = jda.load("app/native/assets/settings.json").value<
			QVariantMap>();

	if (jda.hasError()) {
		qCritical() << "Unable to parse json settings: " << jda.error();
		exit(1);
	}
	QmlDocument::defaultDeclarativeEngine()->rootContext()->setContextProperty(
			"appSettings", appSettings);

	// bbm setup
	const QString uuid(appSettings["bbmUUID"].toString());
	BBMHandler *bbmHandler = new BBMHandler(uuid, app);
	bbmHandler->registerApplication();

	QmlDocument *qml = QmlDocument::create("asset:///main.qml").parent(this);
	qml->setContextProperty("_socialInvocation", this);

	//Expose the BBM Registration handler to main.qml.
	qml->setContextProperty("bbmHandler", bbmHandler);

	//Expose the ApplicationUI in main.qml
	qml->setContextProperty("app", this);

	// Create Active Frame (shown when app is miminized)
	ActiveFrameQML *activeFrame = new ActiveFrameQML();
	Application::instance()->setCover(activeFrame);

	// Create root object for the UI
	AbstractPane *root = qml->createRootObject<AbstractPane>();

	// Set created root object as the application scene
	Application::instance()->setScene(root);
}

// on system language changed
void ApplicationUI::onSystemLanguageChanged() {
	QCoreApplication::instance()->removeTranslator(m_pTranslator);
	// Initiate, load and install the application translation files.
	QString locale_string = QLocale().name();
	QString file_name = QString("Boilerplate_Cascades_%1").arg(locale_string);
	if (m_pTranslator->load(file_name, "app/native/qm")) {
		QCoreApplication::instance()->installTranslator(m_pTranslator);
	}
}

// invoke handling
void ApplicationUI::invoke(const QString &target, const QString &action,
		const QString &mimetype, const QString &uri) {
	// Create a new invocation request
	bb::system::InvokeRequest request;

	request.setTarget(target);
	request.setAction(action);

	if (target == QLatin1String("com.rim.bb.app.facebook")) {
		QVariantMap payload;

		if (!uri.isEmpty()) {
			payload["object_type"] = mimetype;
			payload["object_id"] = uri;
		} else {
			payload["object_type"] = "page";
			payload["object_id"] = "328506290597521";
		}
		request.setMetadata(payload);
	} else {
		request.setUri(uri);
	}
	m_invokeManager->invoke(request);
}

// helper function to encode QStrings
QByteArray ApplicationUI::encodeQString(const QString& toEncode) const {
	return toEncode.toUtf8();
}
