package client

import (
	"bufio"
	"fmt"
	"net"
	"strings"
	"time"
)

// Client represents a connection to the KV storage server
type Client struct {
	Host    string
	Port    int
	Timeout time.Duration
	conn    net.Conn
}

// NewClient creates a new KV client
func NewClient(host string, port int, timeout time.Duration) *Client {
	return &Client{
		Host:    host,
		Port:    port,
		Timeout: timeout,
	}
}

// Connect establishes a connection to the server
func (c *Client) Connect() error {
	addr := net.JoinHostPort(c.Host, fmt.Sprintf("%d", c.Port))

	conn, err := net.DialTimeout("tcp", addr, c.Timeout)
	if err != nil {
		return fmt.Errorf("failed to connect to %s: %w", addr, err)
	}

	c.conn = conn

	// Set read/write deadlines
	if err := c.conn.SetDeadline(time.Now().Add(c.Timeout)); err != nil {
		_ = c.conn.Close()
		return fmt.Errorf("failed to set deadline: %w", err)
	}

	// Read welcome message
	reader := bufio.NewReader(c.conn)
	welcome, err := reader.ReadString('\n')
	if err != nil {
		_ = c.conn.Close()
		return fmt.Errorf("failed to read welcome message: %w", err)
	}

	if !strings.HasPrefix(welcome, "+OK") {
		_ = c.conn.Close()
		return fmt.Errorf("unexpected welcome message: %s", welcome)
	}

	return nil
}

// Close closes the connection
func (c *Client) Close() error {
	if c.conn != nil {
		return c.conn.Close()
	}
	return nil
}

// sendCommand sends a command and reads the response
func (c *Client) sendCommand(cmd string) (*Response, error) {
	if c.conn == nil {
		return nil, fmt.Errorf("not connected to server")
	}

	// Reset deadline for each command
	if err := c.conn.SetDeadline(time.Now().Add(c.Timeout)); err != nil {
		return nil, fmt.Errorf("failed to set deadline: %w", err)
	}

	// Send command with proper formatting
	_, err := fmt.Fprintf(c.conn, "%s", formatCommand(cmd))
	if err != nil {
		return nil, fmt.Errorf("failed to send command: %w", err)
	}

	// Read response
	reader := bufio.NewReader(c.conn)
	line, err := reader.ReadString('\n')
	if err != nil {
		return nil, fmt.Errorf("failed to read response: %w", err)
	}

	return parseResponse(line)
}

// Get retrieves a value by key
func (c *Client) Get(key string) (*Response, error) {
	return c.sendCommand(fmt.Sprintf("%s %s", CommandGet, key))
}

// Set stores a key-value pair
func (c *Client) Set(key, value string) (*Response, error) {
	return c.sendCommand(fmt.Sprintf("%s %s %s", CommandPut, key, value))
}

// Delete removes a key
func (c *Client) Delete(key string) (*Response, error) {
	return c.sendCommand(fmt.Sprintf("%s %s", CommandDelete, key))
}

// Ping checks server connectivity
func (c *Client) Ping() (*Response, error) {
	return c.sendCommand(CommandPing)
}

// Quit sends a quit command to the server
func (c *Client) Quit() (*Response, error) {
	return c.sendCommand(CommandQuit)
}

// Status retrieves server status and replication info
func (c *Client) Status() (*Response, error) {
	return c.sendCommand(CommandStatus)
}
