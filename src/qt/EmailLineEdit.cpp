﻿//EmailLineEdit.cpp by Emercoin developers
#include "EmailLineEdit.h"

EmailLineEdit::EmailLineEdit() {
	_validator = new EmailValidator(this);
	setValidator(_validator);
	setPlaceholderText(tr("Like example@gmail.com"));
}
