/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "services.h"
#include "sockets.h"
#include "socketengine.h"

BufferedSocket::BufferedSocket() {
}

BufferedSocket::~BufferedSocket() {
}

bool BufferedSocket::ProcessRead() {
    char tbuffer[NET_BUFSIZE];

    this->recv_len = 0;

    int len = this->io->Recv(this, tbuffer, sizeof(tbuffer) - 1);
    if (len == 0) {
        return false;
    }
    if (len < 0) {
        return SocketEngine::IgnoreErrno();
    }

    tbuffer[len] = 0;
    this->read_buffer.append(tbuffer);
    this->recv_len = len;

    return true;
}

bool BufferedSocket::ProcessWrite() {
    int count = this->io->Send(this, this->write_buffer);
    if (count == 0) {
        return false;
    }
    if (count < 0) {
        return SocketEngine::IgnoreErrno();
    }

    this->write_buffer = this->write_buffer.substr(count);
    if (this->write_buffer.empty()) {
        SocketEngine::Change(this, false, SF_WRITABLE);
    }

    return true;
}

const Anope::string BufferedSocket::GetLine() {
    size_t s = this->read_buffer.find('\n');
    if (s == Anope::string::npos) {
        return "";
    }
    Anope::string str = this->read_buffer.substr(0, s + 1);
    this->read_buffer.erase(0, s + 1);
    this->read_buffer.ltrim("\r\n");
    return str.trim("\r\n");
}

void BufferedSocket::Write(const char *buffer, size_t l) {
    this->write_buffer += buffer + Anope::string("\r\n");
    SocketEngine::Change(this, true, SF_WRITABLE);
}

void BufferedSocket::Write(const char *message, ...) {
    va_list vi;
    char tbuffer[BUFSIZE];

    if (!message) {
        return;
    }

    va_start(vi, message);
    int len = vsnprintf(tbuffer, sizeof(tbuffer), message, vi);
    va_end(vi);

    this->Write(tbuffer, std::min(len, static_cast<int>(sizeof(tbuffer))));
}

void BufferedSocket::Write(const Anope::string &message) {
    this->Write(message.c_str(), message.length());
}

int BufferedSocket::ReadBufferLen() const {
    return recv_len;
}

int BufferedSocket::WriteBufferLen() const {
    return this->write_buffer.length();
}


BinarySocket::DataBlock::DataBlock(const char *b, size_t l) {
    this->orig = this->buf = new char[l];
    memcpy(this->buf, b, l);
    this->len = l;
}

BinarySocket::DataBlock::~DataBlock() {
    delete [] this->orig;
}

BinarySocket::BinarySocket() {
}

BinarySocket::~BinarySocket() {
}

bool BinarySocket::ProcessRead() {
    char tbuffer[NET_BUFSIZE];

    int len = this->io->Recv(this, tbuffer, sizeof(tbuffer));
    if (len <= 0) {
        return false;
    }

    return this->Read(tbuffer, len);
}

bool BinarySocket::ProcessWrite() {
    if (this->write_buffer.empty()) {
        SocketEngine::Change(this, false, SF_WRITABLE);
        return true;
    }

    DataBlock *d = this->write_buffer.front();

    int len = this->io->Send(this, d->buf, d->len);
    if (len <= -1) {
        return false;
    } else if (static_cast<size_t>(len) == d->len) {
        delete d;
        this->write_buffer.pop_front();
    } else {
        d->buf += len;
        d->len -= len;
    }

    if (this->write_buffer.empty()) {
        SocketEngine::Change(this, false, SF_WRITABLE);
    }

    return true;
}

void BinarySocket::Write(const char *buffer, size_t l) {
    if (l == 0) {
        return;
    }
    this->write_buffer.push_back(new DataBlock(buffer, l));
    SocketEngine::Change(this, true, SF_WRITABLE);
}

void BinarySocket::Write(const char *message, ...) {
    va_list vi;
    char tbuffer[BUFSIZE];

    if (!message) {
        return;
    }

    va_start(vi, message);
    int len = vsnprintf(tbuffer, sizeof(tbuffer), message, vi);
    va_end(vi);

    this->Write(tbuffer, std::min(len, static_cast<int>(sizeof(tbuffer))));
}

void BinarySocket::Write(const Anope::string &message) {
    this->Write(message.c_str(), message.length());
}

bool BinarySocket::Read(const char *buffer, size_t l) {
    return true;
}
