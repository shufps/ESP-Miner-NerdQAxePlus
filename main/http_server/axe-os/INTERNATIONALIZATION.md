# Internationalization (i18n) - ESP-Miner NerdQAxePlus Web Interface

This document explains how to use and extend the translation system for the web interface.

## Overview

The web interface uses **Angular i18n** with **ngx-translate** to manage multi-language translations.

### Supported Languages

- 🇺🇸 **English (en)** - Default language
- 🇫🇷 **French (fr)**
- 🇪🇸 **Spanish (es)**
- 🇩🇪 **German (de)**
- 🇮🇹 **Italian (it)**
- 🇷🇴 **Romanian (ro)**

## File Structure

```
src/assets/i18n/
├── en.json    # English (reference)
├── fr.json    # French
├── es.json    # Spanish
├── de.json    # German
├── it.json    # Italian
└── ro.json    # Romanian
```

## Translation Key Structure

Translations are organized in logical categories:

```json
{
  "COMMON": {
    "LOADING": "Loading...",
    "SAVE": "Save",
    "CANCEL": "Cancel"
  },
  "NAVIGATION": {
    "HOME": "Home",
    "SETTINGS": "Settings"
  },
  "HOME": {
    "HASH_RATE": "Hash Rate",
    "SHARES": "Shares"
  },
  "SETTINGS": {
    "TITLE": "Settings",
    "POOL_SETTINGS": "Pool Settings"
  },
  "UNITS": {
    "HASHRATE": "Gh/s",
    "TEMPERATURE": "°C"
  }
}
```

## Usage in Templates

### Simple Translation
```html
<h1>{{ 'HOME.HASH_RATE' | translate }}</h1>
```

### Translation with Interpolation
```html
<span [nbTooltip]="'HOME.HASH_RATE_TOOLTIP' | translate">
```

### Translation with Parameters
```html
<p>{{ 'WELCOME_MESSAGE' | translate: {name: userName} }}</p>
```

### Conditional Translation
```html
<span>{{
  isConnected ?
  ('HOME.CONNECTED' | translate) :
  ('HOME.DISCONNECTED' | translate)
}}</span>
```

## Usage in TypeScript Components

### Import the Service
```typescript
import { TranslateService } from '@ngx-translate/core';
```

### Inject in Constructor
```typescript
constructor(private translate: TranslateService) {}
```

### Synchronous Translation
```typescript
const message = this.translate.instant('COMMON.SAVE');
```

### Asynchronous Translation
```typescript
this.translate.get('COMMON.SAVE').subscribe(translation => {
  console.log(translation);
});
```

## Language Selector

The language selector is available in the header and uses **NgRx Store** for state management.

### Component: `LanguageSelectorComponent`
- **Location**: `src/app/@i18n/container/language-selector/`
- **Features**:
  - Selection with flags
  - Persistence in localStorage
  - Synchronization with NgRx Store

## Adding a New Language

### 1. Update the Model
In `src/app/@i18n/models/language.model.ts`:
```typescript
export type Language = 'fr' | 'en' | 'it' | 'de' | 'es' | 'ro' | 'pt';
```

### 2. Create Translation File
Create `src/assets/i18n/pt.json` with all translated keys.

### 3. Update the Application
In `src/app/app.component.ts`:
```typescript
translate.addLangs(['en', 'fr', 'es', 'de', 'it', 'ro', 'pt']);
```

### 4. Add Option to Selector
In `language-selector.component.html`:
```html
<nb-option value="pt">
  <span class="flag-icon flag-icon-pt"></span>
  Português
</nb-option>
```

## Adding a New Translation

### 1. Identify the Key
Use a clear convention: `SECTION.ELEMENT` (e.g., `HOME.HASH_RATE`)

### 2. Add to All JSON Files
```json
{
  "HOME": {
    "NEW_FEATURE": "New Feature"
  }
}
```

### 3. Use in Template
```html
<span>{{ 'HOME.NEW_FEATURE' | translate }}</span>
```

## Naming Conventions

- **UPPER_CASE** for keys
- **Dots** for hierarchy: `SECTION.SUBSECTION.ELEMENT`
- **Underscore** to separate words: `HASH_RATE`

### Recommended Categories:
- `COMMON`: Common elements (buttons, actions)
- `NAVIGATION`: Menus and navigation
- `HOME`: Home page
- `SETTINGS`: Settings page
- `SYSTEM`: System information
- `ERRORS`: Error messages
- `SUCCESS`: Success messages
- `UNITS`: Measurement units

## Testing and Validation

### Check for Missing Translations
```bash
# Compare keys between files
npm run i18n:check
```

### Test Language Switching
1. Start the application: `npm start`
2. Use the language selector in the header
3. Verify that all texts change

## Performance

- Translation files are loaded on demand
- The system caches translations
- Language is saved in localStorage

## Troubleshooting

### Translation Not Displayed
1. Check that the key exists in all JSON files
2. Check JSON syntax (commas, quotes)
3. Verify that ngx-translate is configured in the module

### Build Error
- Check that all JSON files are valid
- Ensure imports are correct

## Support and Contribution

To add or correct translations:
1. Edit the corresponding JSON files
2. Test the changes
3. Ensure all languages are consistent

## Resources

- [Angular i18n Guide](https://angular.io/guide/i18n)
- [ngx-translate Documentation](https://github.com/ngx-translate/core)
- [Language Codes ISO 639-1](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes)