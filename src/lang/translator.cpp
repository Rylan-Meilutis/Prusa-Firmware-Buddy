#include "translator.hpp"
#include "translation_provider_empty.hpp"

string_view_utf8 gettext(const char *src) {
    if (!src) {
        return string_view_utf8::MakeNULLSTR();
    }
    if (auto f = Translations::Instance().gettext_hook) {
        f(src);
    }
    return Translations::Instance().CurrentProvider()->GetText(src);
}

namespace {
// register the emptyProvider as "translator" for EN language ... i.e. return the source string intact
ProviderRegistrator prEN("en", EmptyTranslationProvider::Instance());
} // namespace

// this explicit initialization of currentProvider is here to make sure the pointer is always initialized
// even if there were no registered languages at all
Translations::Translations()
    : currentProvider(EmptyTranslationProvider::Instance()) {
    // this is a safety precaution - initialize the array with valid pointers even if the cells are empty
    translations.fill(TranRec({ currentProvider, 0U }));
}

bool Translations::RegisterProvider(uint16_t langCode, const ITranslationProvider *provider) {
    // This tiny table does not benefit from the four-way unrolled find_if.
    // Keep the first free slot while searching for an existing registration.
    TranRec *available = nullptr;
    for (auto &record : translations) {
        if (record.langCode == langCode) {
            record.provider = provider;
            return true;
        }
        if (record.langCode == 0 && !available) {
            available = &record;
        }
    }
    if (!available) {
        return false;
    }
    available->langCode = langCode;
    available->provider = provider;
    return true;
}

bool Translations::ChangeLanguage(uint16_t langCode) {
    if (langCode == 0) {
        return false; // requesting an invalid langCode to change to
    }
    // set current provider pointer to the one specified by lang code
    for (const auto &record : translations) {
        if (record.langCode == langCode) {
            currentProvider = record.provider;
            return true;
        }
    }
    return false;
}

bool Translations::LangExists(uint16_t langCode) const {
    for (const auto &record : translations) {
        if (record.langCode == langCode) {
            return true;
        }
    }
    return false;
}
