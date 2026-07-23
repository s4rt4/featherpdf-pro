// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "backends/FormFiller.h"

#include <QFile>

#include <memory>
#include <vector>

#include <poppler-form.h>
#include <poppler-qt6.h>

namespace {
FormFiller::Kind kindOf(Poppler::FormField* f) {
    switch (f->type()) {
    case Poppler::FormField::FormText:
        return FormFiller::Kind::Text;
    case Poppler::FormField::FormButton:
        switch (static_cast<Poppler::FormFieldButton*>(f)->buttonType()) {
        case Poppler::FormFieldButton::CheckBox: return FormFiller::Kind::CheckBox;
        case Poppler::FormFieldButton::Radio: return FormFiller::Kind::Radio;
        default: return FormFiller::Kind::Unsupported; // push button - nothing to fill
        }
    case Poppler::FormField::FormChoice:
        return static_cast<Poppler::FormFieldChoice*>(f)->choiceType() ==
                       Poppler::FormFieldChoice::ComboBox
                   ? FormFiller::Kind::ComboBox
                   : FormFiller::Kind::ListBox;
    default:
        return FormFiller::Kind::Unsupported;
    }
}
} // namespace

QList<FormFiller::Field> FormFiller::read(const QString& path) {
    QList<Field> out;
    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(path);
    if (!doc || doc->isLocked())
        return out;

    for (int pg = 0; pg < doc->numPages(); ++pg) {
        std::unique_ptr<Poppler::Page> page = doc->page(pg);
        if (!page)
            continue;
        for (const auto& f : page->formFields()) {
            const Kind kind = kindOf(f.get());
            if (kind == Kind::Unsupported)
                continue;
            Field field;
            field.id = f->id();
            field.page = pg;
            field.name = f->fullyQualifiedName();
            field.rect = f->rect();
            field.kind = kind;
            field.readOnly = f->isReadOnly();
            switch (kind) {
            case Kind::Text:
                field.textValue = static_cast<Poppler::FormFieldText*>(f.get())->text();
                break;
            case Kind::CheckBox:
            case Kind::Radio:
                field.checked = static_cast<Poppler::FormFieldButton*>(f.get())->state();
                break;
            case Kind::ComboBox:
            case Kind::ListBox: {
                auto* ch = static_cast<Poppler::FormFieldChoice*>(f.get());
                field.options = ch->choices();
                const QList<int> cur = ch->currentChoices();
                if (!cur.isEmpty() && cur.first() >= 0 && cur.first() < field.options.size())
                    field.currentChoice = field.options.at(cur.first());
                else
                    field.currentChoice = ch->editChoice();
                break;
            }
            default:
                break;
            }
            out.append(field);
        }
    }
    return out;
}

bool FormFiller::save(const QString& inputPath, const QString& outputPath,
                      const QHash<int, QVariant>& values, QString* error) {
    if (values.isEmpty()) {
        if (error)
            *error = QStringLiteral("Nothing to save.");
        return false;
    }

    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(inputPath);
    if (!doc || doc->isLocked()) {
        if (error)
            *error = QStringLiteral("The document couldn't be opened for filling.");
        return false;
    }

    // Keep pages alive while we edit; values are written into the document model.
    std::vector<std::unique_ptr<Poppler::Page>> pages;
    for (int pg = 0; pg < doc->numPages(); ++pg) {
        std::unique_ptr<Poppler::Page> page = doc->page(pg);
        if (!page)
            continue;
        for (const auto& f : page->formFields()) {
            auto it = values.constFind(f->id());
            if (it == values.constEnd())
                continue;
            switch (kindOf(f.get())) {
            case Kind::Text:
                static_cast<Poppler::FormFieldText*>(f.get())->setText(it.value().toString());
                break;
            case Kind::CheckBox:
            case Kind::Radio:
                static_cast<Poppler::FormFieldButton*>(f.get())->setState(it.value().toBool());
                break;
            case Kind::ComboBox:
            case Kind::ListBox: {
                auto* ch = static_cast<Poppler::FormFieldChoice*>(f.get());
                const int idx = ch->choices().indexOf(it.value().toString());
                if (idx >= 0)
                    ch->setCurrentChoices({idx});
                break;
            }
            default:
                break;
            }
        }
        pages.push_back(std::move(page));
    }

    const QString target = outputPath + QStringLiteral(".feather-tmp");
    std::unique_ptr<Poppler::PDFConverter> conv = doc->pdfConverter();
    conv->setOutputFileName(target);
    conv->setPDFOptions(conv->pdfOptions() | Poppler::PDFConverter::WithChanges);
    if (!conv->convert()) {
        if (error)
            *error = QStringLiteral("The filled form couldn't be written.");
        QFile::remove(target);
        return false;
    }

    QFile::remove(outputPath);
    if (!QFile::rename(target, outputPath)) {
        if (error)
            *error = QStringLiteral("The filled form couldn't replace the original.");
        QFile::remove(target);
        return false;
    }
    return true;
}
