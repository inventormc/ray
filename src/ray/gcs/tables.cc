#include "ray/gcs/tables.h"

#include "ray/gcs/client.h"

#include "common_protocol.h"

namespace {

std::shared_ptr<TaskTableDataT> MakeTaskTableData(const TaskExecutionSpec &execution_spec,
                                                  const ClientID &local_scheduler_id,
                                                  SchedulingState scheduling_state) {
  auto data = std::make_shared<TaskTableDataT>();
  data->scheduling_state = scheduling_state;
  data->task_info =
      std::string(execution_spec.Spec(), execution_spec.SpecSize());
  data->scheduler_id = local_scheduler_id.binary();

  flatbuffers::FlatBufferBuilder fbb;
  auto execution_dependencies = CreateTaskExecutionDependencies(
      fbb, to_flatbuf(fbb, execution_spec.ExecutionDependencies()));
  fbb.Finish(execution_dependencies);

  data->execution_dependencies =
      std::string((const char *) fbb.GetBufferPointer(), fbb.GetSize());
  data->spillback_count = execution_spec.SpillbackCount();

  return data;
}

}  // namespace

namespace ray {

namespace gcs {

// TODO(pcm): This is a helper method that should go away once we get rid of
// the Task* datastructure and replace it with TaskTableDataT.
Status TaskTableAdd(AsyncGcsClient *gcs_client, Task *task) {
  TaskExecutionSpec &execution_spec = *Task_task_execution_spec(task);
  TaskSpec *spec = execution_spec.Spec();
  auto data = MakeTaskTableData(execution_spec, Task_local_scheduler(task),
                                static_cast<SchedulingState>(Task_state(task)));
  return gcs_client->task_table().Add(
      ray::JobID::nil(), TaskSpec_task_id(spec), data,
      [](gcs::AsyncGcsClient *client, const TaskID &id,
         std::shared_ptr<TaskTableDataT> data) {});
}

// TODO(pcm): This is a helper method that should go away once we get rid of
// the Task* datastructure and replace it with TaskTableDataT.
Status TaskTableTestAndUpdate(AsyncGcsClient *gcs_client, const TaskID &task_id,
                              const ClientID &local_scheduler_id, int test_state_bitmask,
                              SchedulingState update_state,
                              const TaskTable::TestAndUpdateCallback &callback) {
  auto data = std::make_shared<TaskTableTestAndUpdateT>();
  data->test_scheduler_id = local_scheduler_id.binary();
  data->test_state_bitmask = test_state_bitmask;
  data->update_state = update_state;
  return gcs_client->task_table().TestAndUpdate(ray::JobID::nil(), task_id,
                                                data, callback);
}

void ClientConnected(gcs::AsyncGcsClient *client, const ClientID &client_id,
                     std::shared_ptr<ObjectTableDataT> data) {}

ClientTable::ClientTable(const std::shared_ptr<RedisContext> &context,
                         AsyncGcsClient *client)
    : Table(context, client), client_id_(UniqueID::from_random()) {}

Status ClientTable::Connect(ClientID *client_id) {
  auto data = std::make_shared<ClientTableDataT>();
  data->client_id = client_id_.binary();
  // TODO(swang): Get the address and port from somewhere.
  data->node_manager_address = "";
  data->node_manager_port = 0;
  data->is_insertion = true;

  // TODO(swang):
  // - Add ourselves to the client table (easier if this is synchronous).
  // - Subscribe to the client table.
  // - Once subscription is complete, read all client table entries once.

  *client_id = client_id_;
  return Status::OK();
}

Status ClientTable::Disconnect() {
  auto data = std::make_shared<ClientTableDataT>();
  data->client_id = client_id_.binary();
  // TODO(swang): Get the address and port from somewhere.
  data->node_manager_address = "";
  data->node_manager_port = 0;
  data->is_insertion = false;

  // TODO(swang):
  // - Add ourselves to the client table (easier if this is synchronous).
  return Status::OK();
}

}  // namespace gcs

}  // namespace ray