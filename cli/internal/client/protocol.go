package client

import (
	"fmt"
	"strings"
)

// Response represents a server response
type Response struct {
	Success bool
	Message string
	Value   string // Only used for GET responses
}

// parseResponse parses a server response line
// Response format:
//
//	+OK message      - Success response
//	+VALUE data      - GET response with value
//	-ERR message     - Error response
func parseResponse(line string) (*Response, error) {
	line = strings.TrimSpace(line)

	if len(line) == 0 {
		return nil, fmt.Errorf("empty response")
	}

	switch line[0] {
	case '+':
		// Success response: +OK message or +VALUE data
		parts := strings.SplitN(line[1:], " ", 2)
		if len(parts) < 1 {
			return nil, fmt.Errorf("malformed response: %s", line)
		}

		resp := &Response{Success: true}

		if parts[0] == "VALUE" && len(parts) == 2 {
			// GET response with value
			resp.Value = parts[1]
			resp.Message = "OK"
		} else if len(parts) == 2 {
			// Other success response with message
			resp.Message = parts[1]
		} else {
			// Success response with just status
			resp.Message = parts[0]
		}

		return resp, nil

	case '-':
		// Error response: -ERR message
		parts := strings.SplitN(line[1:], " ", 2)
		if len(parts) < 2 {
			return &Response{Success: false, Message: line[1:]}, nil
		}
		return &Response{Success: false, Message: parts[1]}, nil

	default:
		return nil, fmt.Errorf("unknown response format: %s", line)
	}
}

// formatCommand formats a command string for sending to the server
// All commands are terminated with \r\n
func formatCommand(cmd string) string {
	return cmd + "\r\n"
}

// Command types supported by the protocol
const (
	CommandPut    = "PUT"
	CommandGet    = "GET"
	CommandDelete = "DELETE"
	CommandPing   = "PING"
	CommandQuit   = "QUIT"
)
