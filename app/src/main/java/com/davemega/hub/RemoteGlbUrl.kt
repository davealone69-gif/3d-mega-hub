package com.davemega.hub

import com.davemega.hub.avatarforge.GlbValidator
import java.io.File
import java.net.HttpURLConnection
import java.net.URI
import java.net.URLDecoder
import java.nio.charset.StandardCharsets
import java.util.Locale

/**
 * Normalizes and downloads remote `.glb` assets over HTTPS (including Dropbox share links).
 *
 * Dropbox browser share URLs (`?dl=0` or missing `dl`) return an HTML preview page rather than the
 * binary glTF payload; [normalizeDownloadUrl] rewrites Dropbox share queries to `dl=1` while
 * preserving shared-link tokens (`rlkey`, `st`, etc.) and enforces HTTPS across redirects.
 */
internal object RemoteGlbUrl {
    private const val MAX_URL_LENGTH = 4_096
    private const val MAX_REDIRECTS = 8
    private val loopbackHosts = setOf("localhost", "127.0.0.1")
    private val urlTokenRegex = Regex("""https?://[^\s<>"]+""", RegexOption.IGNORE_CASE)

    fun extractUrl(raw: String): String {
        val trimmed = raw.trim()
        require(trimmed.isNotEmpty()) { "Enter an HTTPS or Dropbox .glb link." }
        val match = urlTokenRegex.find(trimmed)
        return (match?.value ?: trimmed).trimEnd('.', ',', ';', ')', ']')
    }

    fun normalizeDownloadUrl(raw: String): URI {
        val candidate = extractUrl(raw)
        require(candidate.length <= MAX_URL_LENGTH) { "The GLB URL is too long." }
        require(candidate.none { it.isISOControl() || it.isWhitespace() }) {
            "The GLB URL must not contain whitespace or control characters."
        }

        val uri = try {
            URI(candidate)
        } catch (_: Exception) {
            throw IllegalArgumentException("The GLB URL is malformed.")
        }

        validateAllowedUri(uri, allowLoopbackHttp = true)

        val host = uri.host.lowercase(Locale.ROOT)
        if (isDropboxShareHost(host)) {
            val rewrittenQuery = rewriteDropboxQueryToDirectDownload(uri.rawQuery)
            return URI(
                uri.scheme.lowercase(Locale.ROOT),
                null,
                host,
                uri.port,
                uri.rawPath,
                null,
                null
            ).let { base ->
                URI("$base?$rewrittenQuery")
            }
        }
        return uri
    }

    fun validateRedirectTarget(current: URI, locationHeader: String): URI {
        val trimmed = locationHeader.trim()
        require(trimmed.isNotEmpty() && trimmed.length <= MAX_URL_LENGTH) {
            "Remote server returned an invalid redirect location."
        }
        val resolved = try {
            current.resolve(trimmed)
        } catch (_: Exception) {
            throw IllegalArgumentException("Remote server returned a malformed redirect URL.")
        }
        val currentWasLoopback = current.host?.lowercase(Locale.ROOT) in loopbackHosts
        // Never allow a public HTTPS link to redirect to cleartext HTTP or localhost.
        validateAllowedUri(resolved, allowLoopbackHttp = currentWasLoopback)
        if (!currentWasLoopback) {
            val targetHost = resolved.host.lowercase(Locale.ROOT)
            require(targetHost !in loopbackHosts) {
                "Remote HTTPS server is not permitted to redirect to localhost."
            }
        }
        return resolved
    }

    fun deriveDisplayName(uri: URI, contentDisposition: String? = null): String {
        val fromHeader = contentDisposition?.let(::parseContentDispositionFilename)
        val rawCandidate = fromHeader?.takeIf { it.isNotBlank() } ?: run {
            val path = uri.rawPath.orEmpty().trimEnd('/')
            val lastSlash = path.lastIndexOf('/')
            val segment = if (lastSlash >= 0) path.substring(lastSlash + 1) else path
            runCatching { URLDecoder.decode(segment, StandardCharsets.UTF_8.name()) }
                .getOrDefault(segment)
        }
        val sanitized = rawCandidate
            .replace('\\', '_')
            .replace('/', '_')
            .filter { ch -> !ch.isISOControl() && ch != ':' && ch != '*' && ch != '?' && ch != '"' && ch != '<' && ch != '>' && ch != '|' }
            .trim()
            .take(96)
        val base = sanitized.ifEmpty { "remote-model.glb" }
        return if (base.lowercase(Locale.ROOT).endsWith(".glb")) base else "$base.glb"
    }

    fun downloadToFile(
        rawUrl: String,
        destinationTempFile: File,
        maxBytes: Long = GlbValidator.MAX_GLB_BYTES,
        onProgress: (downloadedBytes: Long, totalBytes: Long) -> Unit = { _, _ -> }
    ): String {
        var currentUri = normalizeDownloadUrl(rawUrl)
        var redirects = 0

        while (true) {
            val connection = (currentUri.toURL().openConnection() as HttpURLConnection).apply {
                connectTimeout = 20_000
                readTimeout = 60_000
                requestMethod = "GET"
                instanceFollowRedirects = false
                useCaches = false
                setRequestProperty("User-Agent", "3DMegaHub-AvatarForge/1.0")
                setRequestProperty("Accept", "model/gltf-binary, application/octet-stream, */*")
            }
            try {
                val status = connection.responseCode
                if (status in intArrayOf(301, 302, 303, 307, 308)) {
                    require(redirects < MAX_REDIRECTS) {
                        "Too many HTTP redirects while downloading GLB."
                    }
                    val location = connection.getHeaderField("Location")
                        ?: throw IllegalArgumentException("HTTP $status redirect missing Location header.")
                    currentUri = validateRedirectTarget(currentUri, location)
                    redirects++
                    continue
                }
                if (status !in 200..299) {
                    throw IllegalStateException("Server returned HTTP $status while downloading GLB.")
                }

                val contentLength = connection.contentLengthLong
                if (contentLength > maxBytes) {
                    throw IllegalArgumentException(
                        "Remote GLB (${contentLength / (1024L * 1024L)} MiB) exceeds the ${maxBytes / (1024L * 1024L)} MiB import limit."
                    )
                }

                val disposition = connection.getHeaderField("Content-Disposition")
                val displayName = deriveDisplayName(currentUri, disposition)
                val headBytes = ByteArray(16)
                var headLen = 0

                connection.inputStream.use { source ->
                    destinationTempFile.outputStream().use { target ->
                        val buffer = ByteArray(64 * 1024)
                        var total = 0L
                        while (true) {
                            val count = source.read(buffer)
                            if (count < 0) break
                            if (headLen < headBytes.size) {
                                val copyCount = minOf(count, headBytes.size - headLen)
                                System.arraycopy(buffer, 0, headBytes, headLen, copyCount)
                                headLen += copyCount
                            }
                            total += count
                            require(total <= maxBytes) {
                                "GLB exceeds the ${maxBytes / (1024L * 1024L)} MiB import limit."
                            }
                            target.write(buffer, 0, count)
                            onProgress(total, contentLength)
                        }
                        require(total > 0L) { "Downloaded file was empty." }
                    }
                }

                if (looksLikeHtmlPayload(headBytes, headLen)) {
                    throw IllegalArgumentException(
                        "The URL returned an HTML webpage instead of a binary .glb file. Check that the link points directly to a shared .glb file."
                    )
                }

                GlbValidator.validate(destinationTempFile)
                return displayName
            } finally {
                connection.disconnect()
            }
        }
    }

    private fun validateAllowedUri(uri: URI, allowLoopbackHttp: Boolean) {
        val scheme = uri.scheme?.lowercase(Locale.ROOT)
        require(scheme == "https" || scheme == "http") {
            "Use an https:// URL (or Dropbox share link) to import a remote GLB."
        }
        require(uri.isAbsolute && !uri.isOpaque) { "Enter a complete URL." }
        require(uri.rawUserInfo == null) { "Credentials must not be embedded in the GLB URL." }
        val host = uri.host?.lowercase(Locale.ROOT)
            ?: throw IllegalArgumentException("The GLB URL must include a valid host.")
        val port = uri.port
        require(port == -1 || port in 1..65_535) { "The GLB URL port is invalid." }
        val isLoopback = host in loopbackHosts
        require(scheme == "https" || (allowLoopbackHttp && isLoopback)) {
            "Cleartext HTTP is blocked for remote GLB downloads. Use an https:// link."
        }
    }

    private fun isDropboxShareHost(host: String): Boolean =
        host == "dropbox.com" || host == "www.dropbox.com" || host == "dl.dropbox.com"

    private fun rewriteDropboxQueryToDirectDownload(rawQuery: String?): String {
        if (rawQuery.isNullOrBlank()) return "dl=1"
        val keptParams = rawQuery.split('&').filter { part ->
            if (part.isBlank()) return@filter false
            val key = part.substringBefore('=').lowercase(Locale.ROOT)
            key != "dl" && key != "raw"
        }
        return if (keptParams.isEmpty()) {
            "dl=1"
        } else {
            keptParams.joinToString("&") + "&dl=1"
        }
    }

    private fun parseContentDispositionFilename(header: String): String? {
        val utf8Star = Regex("""filename\*\s*=\s*UTF-8''([^;]+)""", RegexOption.IGNORE_CASE)
            .find(header)?.groupValues?.getOrNull(1)?.trim()?.trim('"')
        if (!utf8Star.isNullOrEmpty()) {
            return runCatching { URLDecoder.decode(utf8Star, StandardCharsets.UTF_8.name()) }
                .getOrDefault(utf8Star)
        }
        return Regex("""filename\s*=\s*"?([^";]+)"?""", RegexOption.IGNORE_CASE)
            .find(header)?.groupValues?.getOrNull(1)?.trim()
    }

    private fun looksLikeHtmlPayload(prefix: ByteArray, length: Int): Boolean {
        if (length <= 0) return false
        val text = String(prefix, 0, length, StandardCharsets.ISO_8859_1).trimStart().lowercase(Locale.ROOT)
        return text.startsWith("<!do") || text.startsWith("<htm") || text.startsWith("<?xm")
    }
}

