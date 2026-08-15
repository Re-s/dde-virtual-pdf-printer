#include <QQmlExtensionPlugin>

class PdfprinterPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    PdfprinterPlugin(QObject *parent = nullptr)
        : QQmlExtensionPlugin(parent)
    {}
    ~PdfprinterPlugin() override {}
    void registerTypes(const char *uri) override
    {
        Q_UNUSED(uri)
        // QML types are registered automatically by the QML compiler
    }
};

#include "qmldir.moc"
