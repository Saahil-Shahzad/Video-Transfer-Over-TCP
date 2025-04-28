/*
    -------------------------
    # A receiver program to #
    # receive a video file  #
    # over TCP sockets      #
    -------------------------
    # written in C          #
    -------------------------
    # 1-12-2024             #
    -------------------------
*/


#include <gtk/gtk.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>

// Declare global variables for the progress bar and status label
GtkWidget *progress_bar;
GtkWidget *status_label;
const char *target_file = "video_received.mp4";  // The name of the file to save the received video

// Structure to hold label widget and text for updating the label
typedef struct {
    GtkWidget *widget;
    const char *text;
} LabelData;

// Structure to hold progress bar widget and fraction for updating progress
typedef struct {
    GtkWidget *widget;
    gdouble fraction;
} ProgressBarData;

// Function to update the label text asynchronously using a GDK idle callback
gboolean update_label(gpointer data) {
    LabelData *label_data = (LabelData *)data;
    gtk_label_set_text(GTK_LABEL(label_data->widget), label_data->text);  // Update the label text
    free(label_data);  // Free the memory allocated for label_data
    return FALSE;
}

// Function to update the progress bar asynchronously using a GDK idle callback
gboolean update_progress_bar(gpointer data) {
    ProgressBarData *pb_data = (ProgressBarData *)data;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pb_data->widget), pb_data->fraction);  // Update progress bar fraction
    free(pb_data);  // Free the memory allocated for progress bar data
    return FALSE;
}

// Function to get the local IP address of the machine
char* get_local_ip() {
    char host[256];
    struct hostent *host_entry;
    char *ip_address;

    gethostname(host, sizeof(host));  // Get the hostname of the machine
    host_entry = gethostbyname(host); // Get the host entry for the machine
    ip_address = inet_ntoa(*(struct in_addr*)host_entry->h_addr_list[0]);  // Convert to IP address string

    return ip_address;
}

// Function to receive video file over TCP socket and update progress
void *receive_video(void *arg) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);  // Create a server socket
    if (server_socket < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(0);  // Let the system choose the port dynamically
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return NULL;
    }

    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    if (listen(server_socket, 1) < 0) {  // Listen for incoming connections
        perror("Listen failed");
        close(server_socket);
        return NULL;
    }

    // Get the assigned port number for the server
    socklen_t len = sizeof(server_address);
    getsockname(server_socket, (struct sockaddr *)&server_address, &len);

    // Get the local IP address
    char *local_ip = get_local_ip();

    // Update the status label to show the IP address and port number
    LabelData *ip_data = malloc(sizeof(LabelData));
    char ip_port_msg[100];
    snprintf(ip_port_msg, sizeof(ip_port_msg), "IP: %s \nWaiting on Port: %d", local_ip, ntohs(server_address.sin_port));
    ip_data->widget = status_label;
    ip_data->text = ip_port_msg;
    gdk_threads_add_idle(update_label, ip_data);  // Update label asynchronously

    int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_len);  // Accept a client connection
    if (client_socket < 0) {
        perror("Accept failed");
        close(server_socket);
        return NULL;
    }

    FILE *file = fopen(target_file, "wb");  // Open the target file to write the received video
    unsigned char buffer[1024];  // Buffer to store data chunks
    size_t received = 0, total_size = 0;

    recv(client_socket, &total_size, sizeof(total_size), 0);  // Receive the total size of the file from the client
    while (1) {
        ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);  // Receive data chunk
        if (bytes_read <= 0) break;  // Exit loop if no more data is received
        fwrite(buffer, 1, bytes_read, file);  // Write the data to the file
        received += bytes_read;  // Update the number of bytes received

        // Update the progress bar with the received data
        ProgressBarData *pb_data = malloc(sizeof(ProgressBarData));
        pb_data->widget = progress_bar;
        pb_data->fraction = (gdouble)received / total_size;  // Calculate the fraction of data received
        gdk_threads_add_idle(update_progress_bar, pb_data);  // Update progress asynchronously
    }

    fclose(file);  // Close the file after writing
    close(client_socket);  // Close the client socket
    close(server_socket);  // Close the server socket

    // Update the status label to indicate that the transfer is complete
    LabelData *complete_data = malloc(sizeof(LabelData));
    complete_data->widget = status_label;
    complete_data->text = "Transfer complete";
    gdk_threads_add_idle(update_label, complete_data);  // Update label asynchronously

    return NULL;
}

// Callback function to start the video reception process when the "Start Receiving" button is clicked
void on_start_button_clicked(GtkWidget *widget, gpointer data) {
    // Reset the progress bar to 0 and update the status label
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    gtk_label_set_text(GTK_LABEL(status_label), "Waiting for connection...");

    pthread_t receiver_thread;
    pthread_create(&receiver_thread, NULL, receive_video, NULL);  // Create a new thread to receive video
    pthread_detach(receiver_thread);  // Detach the thread so it can run independently
}

// Main function to initialize the GTK window and widgets
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);  // Initialize GTK

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);  // Create a new window
    gtk_window_set_title(GTK_WINDOW(window), "Video Receiver");  // Set the window title
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);  // Set the default window size

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);  // Create a vertical box layout
    gtk_container_add(GTK_CONTAINER(window), vbox);  // Add the vertical box to the window

    // Create and add the progress bar widget to the window
    progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 5);

    // Create and add the status label widget to the window
    status_label = gtk_label_new("Waiting for connection...");
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 5);

    // Create and add the start button widget to the window
    GtkWidget *start_button = gtk_button_new_with_label("Start Receiving");
    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_button_clicked), NULL);  // Connect button click to callback
    gtk_box_pack_start(GTK_BOX(vbox), start_button, FALSE, FALSE, 5);

    gtk_widget_show_all(window);  // Show all the widgets in the window

    gtk_main();  // Start the GTK main event loop

    return 0;
}
