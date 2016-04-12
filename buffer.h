#ifndef _BUFFER_H
#define _BUFFER_H

#include <sys/types.h>
#include "protocol-server.h"

/** \file */

/**
 * \brief packet-based Buffer object for network data output.
 *
 * The buffer is pretty much a wrapper for user-level network buffer
 * and writing data asynchronously.
 *
 * To Create a Buffer, use:
 * @code
 *   void *buffer = Buffer.create(0);
 * @endcode
 * The offset sets a pre-sent amount for the first packet.
 *
 * To destroy a Buffer, use:
 * @code
 *   Buffer.destroy(buffer);
 * @endcode
 *
 * To add data to a Buffer use any of:
 *   - `write` will copy the data to a new buffer packet.
 *
 *   - `size_t Buffer.write(void *buffer, void *data, size_t length)`
 *
 *   - `write_move` will take ownership of the data, wrap it in a buffer
 *     packet and free the mempry once the packet was sent.
 *     A NULL packet sent using write_move will close the connection once
 *     it had been reached (all previous data was sent) - same as
 *     `close_when_done`.
 *
 *   - `size_t Buffer.write_move(void *buffer, void *data, size_t length)`
 *
 *   - `write_next` will COPY the data, and place it as the first packet in
 *     the queue. `write_next` will protect the current packet from being
 *     interrupted in the middle and the data will be sent as soon as
 *     possible without cutting any packets in half.
 *
 *   - `size_t Buffer.write_next(void *buffer, void *data, size_t length)`
 *
 * To send data from a Buffer to a socket / pipe (file descriptor) use:
 *     `size_t Buffer.flush(void * buffer, int fd)`
 */
extern const struct BufferClass {
    /**
     * \brief Create a new buffer object.
     *
     * Reserve memory for the core data and creating a mutex.
     * The buffer object should require ~96 bytes (system dependent),
     * including the mutex object.
     */
    void *(*create)(server_pt owner);

    /**
     * \brief Clear the buffer and destroys the buffer object.
     *
     * Release buffer object's core memory and the mutex associated
     * with the buffer.
     */
    void (*destroy)(void *buffer);

    /**
     * \brief Clear all the data in the buffer (freeing the data's memory),
     *        closing any pending files and resets the writing hook.
     */
    void (*clear)(void *buffer);

    /**
     * \brief Set a writing hook (needs to be reset any time the buffer is
     *        cleared).
     *
     * A writing hook will be used instead of the `write` function to send
     * data to the socket. This allows this buffer to be used for special
     * protocol extension or transport layers, such as SSL/TLS.
     *
     * A writing hook is a function that takes in a pointer to the server
     * (the buffer's owner), the socket to which writing should be performed
     * (fd), a pointer to the data to be written and the length of the data
     * to be written:
     *
     * A writing hook should return the number of bytes actually sent from
     * the data buffer (not the number of bytes sent through the socket,
     * but the number of bytes that can be marked as sent).
     *
     * A writing hook should return -1 if the data couldn't be sent and
     * processing should be stop (the connection was lost or suffered a
     * fatal error).
     *
     * A writing hook should return 0 if no data was sent, but the
     * connection should remain open or no fatal error occured.
     * That is:
     * @code
     *   ssize_t writing_hook(server_pt srv, int fd,
     *                        void *data, size_t len) {
     *       int sent = write(fd, data, len);
     *       if (sent < 0 && (errno & (EWOULDBLOCK | EAGAIN | EINTR)))
     *           sent = 0;
     *        return sent;
     *   }
     * @endcode
     */
    void (*set_whook)(void *buffer,
                      ssize_t (*writing_hook)(server_pt srv, int fd,
                                              void *data, size_t len));

    /**
     * \brief Flush the buffer data through the socket
     *
     * @return the number of bytes sent, if any.
     * @return -1 on error.
     */
    ssize_t (*flush)(void *buffer, int fd);

    /**
     * Take ownership of a FILE pointer and buffers the file data chunk
     * by chunk (each chunk will be no more then ~64Kb in size), minimizing
     * memory usage when sending large files.
     *
     * The file will be automatically closed (using fclose) once all the
     * data was sent (or once the buffer is cleared).
     */
    int (*sendfile)(void *buffer, FILE *file);

    /**
     * \brief Create a copy of the data and pushes the copy to the buffer.
     */
    size_t (*write)(void *buffer, void *data, size_t length);

    /**
     * \brief Take ownership of the data and pushes the pointer to the buffer.
     *
     * The buffer will call `free` to deallocate the data once the data was
     * sent.
     */
    size_t (*write_move)(void *buffer, void *data, size_t length);

    /**
     * \brief Create a copy of the data and pushes the copy to the buffer.
     *
     * The data will be pushed as "next in line", meaning that no "packet"
     * or file will be interrapted in the middle (a packet is data scheduled
     * to be sent using `write`/`write_move`/etc').
     */
    size_t (*write_next)(void *buffer, void *data, size_t length);

    /**
     * \brief Take ownership of the data and pushes the pointer to the buffer.
     *
     * The buffer will call `free` to deallocate the data once the data was
     * sent.
     *
     * The data will be pushed as "next in line", meaning that no "packet"
     * or file will be interrapted in the middle (a packet is data scheduled
     * to be sent using `write`/`write_move`/etc').
     */
    size_t (*write_move_next)(void *buffer, void *data, size_t length);

    /**
     * \brief Mark the connection to closes once the current
     *        buffer data was sent.
     */
    void (*close_when_done)(void *buffer, int fd);

    /**
     * @return true (1) if the buffer is empty.
     */
    char (*is_empty)(void *buffer);
} Buffer;

#endif