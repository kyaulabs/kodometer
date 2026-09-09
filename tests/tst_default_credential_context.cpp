#include <QtTest>
#include <kodometer/deepseek_provider_adapter.hpp>
#include <kodometer/kimi_provider_adapter.hpp>
#include <kodometer/openrouter_provider_adapter.hpp>
#include <kodometer/xai_provider_adapter.hpp>
#include <kodometer/zai_provider_adapter.hpp>

class DefaultCredentialContextTest : public QObject
{
    Q_OBJECT
  private:
    template <class Adapter> void check(Adapter &adapter, const QString &key)
    {
        adapter.setEnvironment({});
        const QByteArray empty = adapter.defaultCredentialContext();
        adapter.setCredentialOverrides({{key, QStringLiteral("synthetic-wallet-key")}});
        const QByteArray wallet = adapter.defaultCredentialContext();
        QVERIFY(wallet != empty);
        QVERIFY(!wallet.contains("synthetic-wallet-key"));
        adapter.setCredentialOverrides({{key, QStringLiteral("replacement-key")}});
        QVERIFY(adapter.defaultCredentialContext() != wallet);
        adapter.setEnvironment({{key, QStringLiteral("environment-key")}});
        const QByteArray environment = adapter.defaultCredentialContext();
        adapter.setCredentialOverrides({{key, QStringLiteral("ignored-wallet-key")}});
        QCOMPARE(adapter.defaultCredentialContext(), environment);
    }
  private slots:
    void respectsEffectiveEnvironmentPrecedence()
    {
        Kodometer::DeepSeekProviderAdapter deepseek;
        Kodometer::KimiProviderAdapter kimi;
        Kodometer::OpenRouterProviderAdapter openrouter;
        Kodometer::XaiProviderAdapter xai;
        Kodometer::ZaiProviderAdapter zai;
        check(deepseek, QStringLiteral("DEEPSEEK_API_KEY"));
        check(kimi, QStringLiteral("KIMI_CODE_API_KEY"));
        check(openrouter, QStringLiteral("OPENROUTER_API_KEY"));
        check(xai, QStringLiteral("XAI_MANAGEMENT_API_KEY"));
        check(zai, QStringLiteral("Z_AI_API_KEY"));
    }
};
QTEST_GUILESS_MAIN(DefaultCredentialContextTest)
#include "tst_default_credential_context.moc"
